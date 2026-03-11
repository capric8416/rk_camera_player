#include "drm_display.hpp"
#include "drm_interface.hpp"
#include "logger.hpp"
#include "system_interface.hpp"

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace player {

DrmDisplay::DrmDisplay(SystemInterface *sys, DrmInterface *drm)
    : m_sys(sys)
    , m_drm(drm)
    , m_own_sys(false)
    , m_own_drm(false)
    , m_fd(-1)
    , m_crtc_id(0)
    , m_crtc_index(0)
    , m_plane_id(0)
    , m_display_width(0)
    , m_display_height(0)
    , m_video_width(0)
    , m_video_height(0)
    , m_front_buf(0)
    , m_initialized(false)
{
    if (!m_sys) {
        m_sys = new LinuxSystem();
        m_own_sys = true;
    }
    if (!m_drm) {
        m_drm = new LibDrm();
        m_own_drm = true;
    }
    memset(m_buffers, 0, sizeof(m_buffers));
}

DrmDisplay::~DrmDisplay()
{
    close();
    if (m_own_sys)
        delete m_sys;
    if (m_own_drm)
        delete m_drm;
}

bool DrmDisplay::open(const std::string &device)
{
    m_fd = m_sys->open(device.c_str(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        LOG_ERROR("无法打开 DRM: {}", device);
        return false;
    }

    m_drm->set_client_cap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    return true;
}

bool DrmDisplay::init(
    uint32_t video_width,
    uint32_t video_height,
    int zpos,
    uint32_t plane_id
)
{
    // 查找活动 CRTC
    drmModeRes *res = m_drm->get_resources(m_fd);
    if (!res) {
        LOG_ERROR("drmModeGetResources 失败");
        return false;
    }

    for (int i = 0; i < res->count_crtcs; ++i) {
        drmModeCrtc *crtc = m_drm->get_crtc(m_fd, res->crtcs[i]);
        if (crtc && crtc->mode_valid) {
            m_crtc_id = crtc->crtc_id;
            m_crtc_index = i;
            m_display_width = crtc->mode.hdisplay;
            m_display_height = crtc->mode.vdisplay;
            LOG_INFO(
                "DRM CRTC: {} ({}x{})",
                m_crtc_id,
                m_display_width,
                m_display_height
            );
            m_drm->free_crtc(crtc);
            break;
        }
        if (crtc)
            m_drm->free_crtc(crtc);
    }
    m_drm->free_resources(res);

    if (!m_crtc_id) {
        LOG_ERROR("无活动 CRTC");
        return false;
    }

    // 查找 overlay plane
    drmModePlaneRes *pres = m_drm->get_plane_resources(m_fd);
    if (!pres) {
        LOG_ERROR("drmModeGetPlaneResources 失败");
        return false;
    }

    if (plane_id > 0) {
        m_plane_id = plane_id;
        LOG_INFO("使用指定 plane: {}", m_plane_id);
    } else {
        for (uint32_t i = 0; i < pres->count_planes; ++i) {
            drmModePlane *p = m_drm->get_plane(m_fd, pres->planes[i]);
            if (p && (p->possible_crtcs & (1 << m_crtc_index)) &&
                p->crtc_id == 0) {
                m_plane_id = p->plane_id;
                LOG_INFO("Overlay plane: {}", m_plane_id);
                m_drm->free_plane(p);
                break;
            }
            if (p)
                m_drm->free_plane(p);
        }
    }
    m_drm->free_plane_resources(pres);

    if (!m_plane_id) {
        LOG_ERROR("无空闲 overlay plane");
        return false;
    }

    m_video_width = video_width > 0 ? video_width : m_display_width;
    m_video_height = video_height > 0 ? video_height : m_display_height;

    // 创建双缓冲
    for (int i = 0; i < 2; ++i) {
        if (!create_buffer(
                m_video_width,
                m_video_height,
                DRM_FORMAT_ABGR8888,
                m_buffers[i]
            )) {
            LOG_ERROR("创建 DRM buffer 失败");
            return false;
        }
        if (m_buffers[i].map) {
            memset(m_buffers[i].map, 0, m_buffers[i].size);
        }
    }

    // 配置 plane 属性
    set_property(m_plane_id, DRM_MODE_OBJECT_PLANE, "zpos", zpos);
    set_property(m_plane_id, DRM_MODE_OBJECT_PLANE, "alpha", 65535);

    // 设置其他 plane 的 zpos 更高 (让 Weston 在前面)
    drmModePlaneRes *pres2 = m_drm->get_plane_resources(m_fd);
    if (pres2) {
        for (uint32_t i = 0; i < pres2->count_planes; ++i) {
            uint32_t other_plane = pres2->planes[i];
            if (other_plane != m_plane_id) {
                if (set_property(
                        other_plane,
                        DRM_MODE_OBJECT_PLANE,
                        "zpos",
                        zpos + 1
                    ) == 0) {
                    LOG_INFO(
                        "设置 plane {} zpos={} (前景)",
                        other_plane,
                        zpos + 1
                    );
                }
            }
        }
        m_drm->free_plane_resources(pres2);
    }

    // 初始帧
    int ret = m_drm->set_plane(
        m_fd,
        m_plane_id,
        m_crtc_id,
        m_buffers[0].fb_id,
        0,
        0,
        0,
        m_display_width,
        m_display_height,
        0,
        0,
        m_video_width << 16,
        m_video_height << 16
    );
    if (ret < 0) {
        LOG_ERROR("drmModeSetPlane 失败: {}", ret);
        return false;
    }

    m_initialized = true;
    LOG_INFO("DRM overlay 初始化: {}x{}", m_video_width, m_video_height);
    return true;
}

void DrmDisplay::close()
{
    if (m_plane_id && m_fd >= 0) {
        m_drm->set_plane(m_fd, m_plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    for (int i = 0; i < 2; ++i) {
        destroy_buffer(m_buffers[i]);
    }

    // 清理 NV12 缓存
    for (auto &pair : m_nv12_cache) {
        if (pair.second.fb_id) {
            m_drm->rm_fb(m_fd, pair.second.fb_id);
        }
        if (pair.second.gem_handle) {
            struct drm_gem_close close_req = {};
            close_req.handle = pair.second.gem_handle;
            m_sys->ioctl(m_fd, DRM_IOCTL_GEM_CLOSE, &close_req);
        }
    }
    m_nv12_cache.clear();

    if (m_fd >= 0) {
        m_sys->close(m_fd);
        m_fd = -1;
    }

    m_initialized = false;
}


bool DrmDisplay::create_buffer(
    uint32_t width,
    uint32_t height,
    uint32_t format,
    Buffer &buf
)
{
    struct drm_mode_create_dumb create = {};
    create.width = width;
    create.height = height;
    create.bpp = 32;

    if (m_sys->ioctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        return false;
    }

    buf.handle = create.handle;
    buf.size = create.size;

    uint32_t handles[4] = {create.handle, 0, 0, 0};
    uint32_t pitches[4] = {create.pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};

    if (m_drm->add_fb2(
            m_fd,
            width,
            height,
            format,
            handles,
            pitches,
            offsets,
            &buf.fb_id,
            0
        ) < 0) {
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = create.handle;
        m_sys->ioctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return false;
    }

    struct drm_mode_map_dumb map = {};
    map.handle = create.handle;

    if (m_sys->ioctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        m_drm->rm_fb(m_fd, buf.fb_id);
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = create.handle;
        m_sys->ioctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return false;
    }

    buf.map = m_sys->mmap(
        nullptr,
        create.size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        m_fd,
        map.offset
    );
    if (buf.map == MAP_FAILED) {
        buf.map = nullptr;
    }

    return true;
}

void DrmDisplay::destroy_buffer(Buffer &buf)
{
    if (buf.map) {
        m_sys->munmap(buf.map, buf.size);
        buf.map = nullptr;
    }

    if (buf.fb_id) {
        m_drm->rm_fb(m_fd, buf.fb_id);
        buf.fb_id = 0;
    }

    if (buf.handle) {
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = buf.handle;
        m_sys->ioctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        buf.handle = 0;
    }
}

int DrmDisplay::set_property(
    uint32_t object_id,
    uint32_t object_type,
    const char *prop_name,
    uint64_t value
)
{
    drmModeObjectProperties *props =
        m_drm->get_object_properties(m_fd, object_id, object_type);
    if (!props) {
        return -1;
    }

    int ret = -1;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyRes *prop = m_drm->get_property(m_fd, props->props[i]);
        if (prop && strcmp(prop->name, prop_name) == 0) {
            ret = m_drm->set_object_property(
                m_fd,
                object_id,
                object_type,
                prop->prop_id,
                value
            );
            m_drm->free_property(prop);
            break;
        }
        if (prop)
            m_drm->free_property(prop);
    }

    m_drm->free_object_properties(props);
    return ret;
}

bool DrmDisplay::display_nv12(
    int fd,
    uint32_t width,
    uint32_t height,
    uint32_t hor_stride,
    uint32_t ver_stride
)
{
    if (!m_initialized || fd < 0) {
        return false;
    }

    uint32_t fb_id = 0;

    // 查找缓存
    auto it = m_nv12_cache.find(fd);
    if (it != m_nv12_cache.end()) {
        // 缓存命中，直接使用
        fb_id = it->second.fb_id;
    } else {
        // 缓存未命中，创建新的 framebuffer
        uint32_t gem_handle = 0;
        int ret = m_drm->prime_fd_to_handle(m_fd, fd, &gem_handle);
        if (ret < 0) {
            LOG_ERROR("drmPrimeFDToHandle 失败: {}", ret);
            return false;
        }

        // NV12 格式: Y 平面 + UV 平面
        uint32_t handles[4] = {gem_handle, gem_handle, 0, 0};
        uint32_t pitches[4] = {hor_stride, hor_stride, 0, 0};
        uint32_t offsets[4] = {0, hor_stride * ver_stride, 0, 0};

        ret = m_drm->add_fb2(
            m_fd,
            width,
            height,
            DRM_FORMAT_NV12,
            handles,
            pitches,
            offsets,
            &fb_id,
            0
        );
        if (ret < 0) {
            LOG_ERROR("drmModeAddFB2 (NV12) 失败: {}", ret);
            struct drm_gem_close close_req = {};
            close_req.handle = gem_handle;
            m_sys->ioctl(m_fd, DRM_IOCTL_GEM_CLOSE, &close_req);
            return false;
        }

        // 加入缓存
        m_nv12_cache[fd] = {gem_handle, fb_id};
        m_video_width = width;
        m_video_height = height;
    }

    // 等待 VBlank
    drmVBlank vbl = {};
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;
    m_drm->wait_vblank(m_fd, &vbl);

    // 显示 framebuffer
    int ret = m_drm->set_plane(
        m_fd,
        m_plane_id,
        m_crtc_id,
        fb_id,
        0,
        0,
        0,
        m_display_width,
        m_display_height,
        0,
        0,
        width << 16,
        height << 16
    );

    if (ret < 0) {
        LOG_ERROR("drmModeSetPlane (NV12) 失败: {}", ret);
        return false;
    }

    return true;
}

} // namespace player
