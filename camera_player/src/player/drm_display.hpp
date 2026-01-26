#pragma once

/**
 * DrmDisplay - DRM Overlay 显示
 */

#include <cstdint>
#include <string>
#include <unordered_map>

namespace player {

class DrmDisplay
{
public:
    DrmDisplay();
    ~DrmDisplay();

    // 禁止拷贝
    DrmDisplay(const DrmDisplay &) = delete;
    DrmDisplay &operator=(const DrmDisplay &) = delete;

    bool open(const std::string &device);

    bool init(
        uint32_t video_width,
        uint32_t video_height,
        int zpos,
        uint32_t plane_id = 0
    );

    void close();

    /**
     * 获取后台缓冲区
     */
    void *get_back_buffer(size_t *size);

    /**
     * 翻页显示
     */
    void flip();

    /**
     * 直接显示 NV12 dma-buf
     * @param fd dma-buf 文件描述符
     * @param width 图像宽度
     * @param height 图像高度
     * @param hor_stride 水平步进
     * @param ver_stride 垂直步进
     * @return 成功返回 true
     */
    bool display_nv12(
        int fd,
        uint32_t width,
        uint32_t height,
        uint32_t hor_stride,
        uint32_t ver_stride
    );

    uint32_t display_width() const { return m_display_width; }
    uint32_t display_height() const { return m_display_height; }
    uint32_t video_width() const { return m_video_width; }
    uint32_t video_height() const { return m_video_height; }

private:
    struct Buffer
    {
        uint32_t handle;
        uint32_t fb_id;
        uint32_t size;
        void *map;
    };

    // NV12 framebuffer 缓存项
    struct NV12Buffer
    {
        uint32_t gem_handle;
        uint32_t fb_id;
    };

    bool create_buffer(
        uint32_t width,
        uint32_t height,
        uint32_t format,
        Buffer &buf
    );
    void destroy_buffer(Buffer &buf);
    int set_property(
        uint32_t object_id,
        uint32_t object_type,
        const char *prop_name,
        uint64_t value
    );

    int m_fd;
    uint32_t m_crtc_id;
    uint32_t m_crtc_index;
    uint32_t m_plane_id;
    uint32_t m_display_width;
    uint32_t m_display_height;
    uint32_t m_video_width;
    uint32_t m_video_height;
    Buffer m_buffers[2];
    int m_front_buf;
    bool m_initialized;

    // NV12 直接显示缓存 (fd -> NV12Buffer)
    std::unordered_map<int, NV12Buffer> m_nv12_cache;
};

} // namespace player
