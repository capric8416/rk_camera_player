#include "rga_scale.hpp"
#include "logger.hpp"

#include <rga/im2d.h>
#include <rga/RgaUtils.h>

namespace player {

RgaScaler::RgaScaler()
    : m_src_width(0)
    , m_src_height(0)
    , m_dst_width(0)
    , m_dst_height(0)
    , m_initialized(false)
{
}

RgaScaler::~RgaScaler() {
    destroy();
}

bool RgaScaler::init(
    uint32_t src_width,
    uint32_t src_height,
    uint32_t dst_width,
    uint32_t dst_height
) {
    m_src_width = src_width;
    m_src_height = src_height;
    m_dst_width = dst_width;
    m_dst_height = dst_height;
    m_initialized = true;

    LOG_INFO("RGA 初始化: {}x{} -> {}x{}", m_src_width, m_src_height, m_dst_width, m_dst_height);
    return true;
}

void RgaScaler::destroy() {
    m_initialized = false;
}

bool RgaScaler::convert(
    const DecodedFrame &src,
    void *dst_data,
    size_t dst_size
) {
    if (!m_initialized) {
        return false;
    }

    (void)dst_size;

    // 设置源图像 (NV12)
    rga_buffer_t src_buf = {};
    src_buf.fd = src.fd;
    src_buf.width = static_cast<int>(src.width);
    src_buf.height = static_cast<int>(src.height);
    src_buf.wstride = static_cast<int>(src.hor_stride);
    src_buf.hstride = static_cast<int>(src.ver_stride);
    src_buf.format = RK_FORMAT_YCbCr_420_SP;  // NV12

    // 设置目标图像 (RGBA)
    rga_buffer_t dst_buf = {};
    dst_buf.vir_addr = dst_data;
    dst_buf.width = static_cast<int>(m_dst_width);
    dst_buf.height = static_cast<int>(m_dst_height);
    dst_buf.wstride = static_cast<int>(m_dst_width);
    dst_buf.hstride = static_cast<int>(m_dst_height);
    dst_buf.format = RK_FORMAT_RGBA_8888;

    // 执行颜色空间转换
    IM_STATUS status = imcvtcolor(
        src_buf,
        dst_buf,
        src_buf.format,
        dst_buf.format
    );

    if (status != IM_STATUS_SUCCESS) {
        LOG_ERROR("RGA imcvtcolor 失败: {}", imStrError(status));
        return false;
    }

    return true;
}

} // namespace player
