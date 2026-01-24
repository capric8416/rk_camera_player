#pragma once

/**
 * RgaScaler - RGA 颜色空间转换/缩放
 */

#include "mpp_decode.hpp"
#include <cstdint>
#include <cstddef>

namespace player {

class RgaScaler {
public:
    RgaScaler();
    ~RgaScaler();

    // 禁止拷贝
    RgaScaler(const RgaScaler &) = delete;
    RgaScaler &operator=(const RgaScaler &) = delete;

    bool init(
        uint32_t src_width,
        uint32_t src_height,
        uint32_t dst_width,
        uint32_t dst_height
    );
    void destroy();

    /**
     * 转换 NV12 -> RGBA
     * @param src 解码帧
     * @param dst_data 目标缓冲区
     * @param dst_size 目标缓冲区大小
     * @return 成功返回 true
     */
    bool convert(
        const DecodedFrame &src,
        void *dst_data,
        size_t dst_size
    );

private:
    uint32_t m_src_width;
    uint32_t m_src_height;
    uint32_t m_dst_width;
    uint32_t m_dst_height;
    bool m_initialized;
};

} // namespace player
