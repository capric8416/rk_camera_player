#include "camera_capture.hpp"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cerrno>
#include <cstring>

namespace player {

CameraCapture::CameraCapture()
    : m_fd(-1)
    , m_width(0)
    , m_height(0)
    , m_fps(0)
    , m_current_buffer_index(-1)
    , m_streaming(false)
{
}

CameraCapture::~CameraCapture() {
    close();
}

bool CameraCapture::open(
    const std::string &device,
    uint32_t width,
    uint32_t height,
    uint32_t fps
) {
    m_fd = ::open(device.c_str(), O_RDWR);
    if (m_fd < 0) {
        LOG_ERROR("无法打开摄像头 {}: {}", device, strerror(errno));
        return false;
    }

    // 查询能力
    struct v4l2_capability cap = {};
    if (ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR("VIDIOC_QUERYCAP 失败");
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR("设备不支持视频采集");
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_ERROR("设备不支持流式传输");
        close();
        return false;
    }

    LOG_INFO("摄像头: {} ({})", reinterpret_cast<const char *>(cap.card),
             reinterpret_cast<const char *>(cap.driver));

    // 设置格式 (MJPEG)
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("VIDIOC_S_FMT 失败");
        close();
        return false;
    }

    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
    LOG_INFO("分辨率: {}x{}", m_width, m_height);

    // 设置帧率
    struct v4l2_streamparm parm = {};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (ioctl(m_fd, VIDIOC_S_PARM, &parm) < 0) {
        LOG_WARN("设置帧率失败");
    }

    m_fps = parm.parm.capture.timeperframe.denominator /
            parm.parm.capture.timeperframe.numerator;
    LOG_INFO("帧率: {} fps", m_fps);

    // 申请缓冲区
    struct v4l2_requestbuffers req = {};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("VIDIOC_REQBUFS 失败");
        close();
        return false;
    }

    m_buffers.resize(req.count);

    for (uint32_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QUERYBUF 失败");
            close();
            return false;
        }

        m_buffers[i].size = buf.length;
        m_buffers[i].addr = mmap(
            nullptr,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            m_fd,
            buf.m.offset
        );

        if (m_buffers[i].addr == MAP_FAILED) {
            LOG_ERROR("mmap 失败");
            close();
            return false;
        }
    }

    return true;
}

void CameraCapture::close() {
    stop_streaming();

    for (auto &buf : m_buffers) {
        if (buf.addr && buf.addr != MAP_FAILED) {
            munmap(buf.addr, buf.size);
        }
    }
    m_buffers.clear();

    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool CameraCapture::start_streaming() {
    if (m_streaming) {
        return true;
    }

    // 入队所有缓冲区
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = static_cast<uint32_t>(i);

        if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QBUF 失败");
            return false;
        }
    }

    // 开始流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("VIDIOC_STREAMON 失败");
        return false;
    }

    m_streaming = true;
    return true;
}

void CameraCapture::stop_streaming() {
    if (!m_streaming) {
        return;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(m_fd, VIDIOC_STREAMOFF, &type);
    m_streaming = false;
}

bool CameraCapture::capture_frame(
    uint8_t **data,
    size_t *size
) {
    if (!m_streaming) {
        return false;
    }

    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
        LOG_ERROR("VIDIOC_DQBUF 失败: {}", strerror(errno));
        return false;
    }

    m_current_buffer_index = static_cast<int>(buf.index);
    *data = static_cast<uint8_t *>(m_buffers[buf.index].addr);
    *size = buf.bytesused;

    return true;
}

void CameraCapture::release_frame() {
    if (m_current_buffer_index < 0) {
        return;
    }

    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = static_cast<uint32_t>(m_current_buffer_index);

    ioctl(m_fd, VIDIOC_QBUF, &buf);
    m_current_buffer_index = -1;
}

} // namespace player
