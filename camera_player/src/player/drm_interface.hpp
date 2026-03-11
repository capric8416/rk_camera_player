#pragma once

#include <cstdint>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace player {

/**
 * DrmInterface - DRM 系统调用接口
 *
 * 用于封装 libdrm 函数，方便单元测试进行 Mock
 */
class DrmInterface
{
public:
    virtual ~DrmInterface() = default;

    virtual drmModeResPtr get_resources(int fd) = 0;
    virtual void free_resources(drmModeResPtr ptr) = 0;

    virtual drmModeCrtcPtr get_crtc(int fd, uint32_t crtc_id) = 0;
    virtual void free_crtc(drmModeCrtcPtr ptr) = 0;

    virtual drmModePlaneResPtr get_plane_resources(int fd) = 0;
    virtual void free_plane_resources(drmModePlaneResPtr ptr) = 0;

    virtual drmModePlanePtr get_plane(int fd, uint32_t plane_id) = 0;
    virtual void free_plane(drmModePlanePtr ptr) = 0;

    virtual int set_client_cap(int fd, uint64_t capability, uint64_t value) = 0;

    virtual int set_plane(
        int fd,
        uint32_t plane_id,
        uint32_t crtc_id,
        uint32_t fb_id,
        uint32_t flags,
        int32_t crtc_x,
        int32_t crtc_y,
        uint32_t crtc_w,
        uint32_t crtc_h,
        uint32_t src_x,
        uint32_t src_y,
        uint32_t src_w,
        uint32_t src_h
    ) = 0;

    virtual int add_fb2(
        int fd,
        uint32_t width,
        uint32_t height,
        uint32_t pixel_format,
        uint32_t bo_handles[4],
        uint32_t pitches[4],
        uint32_t offsets[4],
        uint32_t *buf_id,
        uint32_t flags
    ) = 0;

    virtual int rm_fb(int fd, uint32_t buffer_id) = 0;

    virtual int wait_vblank(int fd, drmVBlankPtr vbl) = 0;

    virtual int prime_fd_to_handle(int fd, int prime_fd, uint32_t *handle) = 0;

    virtual drmModeObjectPropertiesPtr
    get_object_properties(int fd, uint32_t object_id, uint32_t object_type) = 0;
    virtual void free_object_properties(drmModeObjectPropertiesPtr ptr) = 0;

    virtual drmModePropertyPtr
    get_property(int fd, uint32_t property_id) = 0;
    virtual void free_property(drmModePropertyPtr ptr) = 0;

    virtual int set_object_property(
        int fd,
        uint32_t object_id,
        uint32_t object_type,
        uint32_t property_id,
        uint64_t value
    ) = 0;
};

/**
 * LibDrm - 真实的 libdrm 接口实现
 */
class LibDrm : public DrmInterface
{
public:
    drmModeResPtr get_resources(int fd) override
    {
        return drmModeGetResources(fd);
    }
    void free_resources(drmModeResPtr ptr) override
    {
        drmModeFreeResources(ptr);
    }

    drmModeCrtcPtr get_crtc(int fd, uint32_t crtc_id) override
    {
        return drmModeGetCrtc(fd, crtc_id);
    }
    void free_crtc(drmModeCrtcPtr ptr) override { drmModeFreeCrtc(ptr); }

    drmModePlaneResPtr get_plane_resources(int fd) override
    {
        return drmModeGetPlaneResources(fd);
    }
    void free_plane_resources(drmModePlaneResPtr ptr) override
    {
        drmModeFreePlaneResources(ptr);
    }

    drmModePlanePtr get_plane(int fd, uint32_t plane_id) override
    {
        return drmModeGetPlane(fd, plane_id);
    }
    void free_plane(drmModePlanePtr ptr) override { drmModeFreePlane(ptr); }

    int set_client_cap(int fd, uint64_t capability, uint64_t value) override
    {
        return drmSetClientCap(fd, capability, value);
    }

    int set_plane(
        int fd,
        uint32_t plane_id,
        uint32_t crtc_id,
        uint32_t fb_id,
        uint32_t flags,
        int32_t crtc_x,
        int32_t crtc_y,
        uint32_t crtc_w,
        uint32_t crtc_h,
        uint32_t src_x,
        uint32_t src_y,
        uint32_t src_w,
        uint32_t src_h
    ) override
    {
        return drmModeSetPlane(
            fd,
            plane_id,
            crtc_id,
            fb_id,
            flags,
            crtc_x,
            crtc_y,
            crtc_w,
            crtc_h,
            src_x,
            src_y,
            src_w,
            src_h
        );
    }

    int add_fb2(
        int fd,
        uint32_t width,
        uint32_t height,
        uint32_t pixel_format,
        uint32_t bo_handles[4],
        uint32_t pitches[4],
        uint32_t offsets[4],
        uint32_t *buf_id,
        uint32_t flags
    ) override
    {
        return drmModeAddFB2(
            fd,
            width,
            height,
            pixel_format,
            bo_handles,
            pitches,
            offsets,
            buf_id,
            flags
        );
    }

    int rm_fb(int fd, uint32_t buffer_id) override
    {
        return drmModeRmFB(fd, buffer_id);
    }

    int wait_vblank(int fd, drmVBlankPtr vbl) override
    {
        return drmWaitVBlank(fd, vbl);
    }

    int prime_fd_to_handle(int fd, int prime_fd, uint32_t *handle) override
    {
        return drmPrimeFDToHandle(fd, prime_fd, handle);
    }

    drmModeObjectPropertiesPtr get_object_properties(
        int fd,
        uint32_t object_id,
        uint32_t object_type
    ) override
    {
        return drmModeObjectGetProperties(fd, object_id, object_type);
    }
    void free_object_properties(drmModeObjectPropertiesPtr ptr) override
    {
        drmModeFreeObjectProperties(ptr);
    }

    drmModePropertyPtr get_property(int fd, uint32_t property_id) override
    {
        return drmModeGetProperty(fd, property_id);
    }
    void free_property(drmModePropertyPtr ptr) override
    {
        drmModeFreeProperty(ptr);
    }

    int set_object_property(
        int fd,
        uint32_t object_id,
        uint32_t object_type,
        uint32_t property_id,
        uint64_t value
    ) override
    {
        return drmModeObjectSetProperty(
            fd,
            object_id,
            object_type,
            property_id,
            value
        );
    }
};

} // namespace player
