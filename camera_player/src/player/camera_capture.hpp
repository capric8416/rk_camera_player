#pragma once

/**
 * CameraCapture - V4L2 摄像头采集
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace player {

class SystemInterface;

class CameraCapture
{
public:
    CameraCapture(SystemInterface *sys = nullptr);
    ~CameraCapture();

    // 禁止拷贝
    CameraCapture(const CameraCapture &) = delete;
    CameraCapture &operator=(const CameraCapture &) = delete;

    bool open(
        const std::string &device,
        uint32_t width,
        uint32_t height,
        uint32_t fps
    );
    void close();

    bool start_streaming();
    void stop_streaming();

    /**
     * 获取一帧 (阻塞等待)
     * @param data 输出帧数据指针
     * @param size 输出帧数据大小
     * @return 成功返回 true
     */
    bool capture_frame(uint8_t **data, size_t *size);

    /**
     * 释放当前帧
     */
    void release_frame();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t fps() const { return m_fps; }

private:
    struct Buffer
    {
        void *addr;
        size_t size;
    };

    int m_fd;
    SystemInterface *m_sys;
    bool m_own_sys;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_fps;
    std::vector<Buffer> m_buffers;
    int m_current_buffer_index;
    bool m_streaming;
};

} // namespace player
