#pragma once

/**
 * MppDecoder - MPP 解码器
 * 
 * 支持 MJPEG, H.264, H.265 解码
 */

#include <cstdint>
#include <cstddef>
#include <rockchip/rk_type.h>

namespace player {

struct DecodedFrame {
    int fd;
    void *data;
    uint32_t width;
    uint32_t height;
    uint32_t hor_stride;
    uint32_t ver_stride;
};

class MppDecoder {
public:
    MppDecoder();
    ~MppDecoder();

    // 禁止拷贝
    MppDecoder(const MppDecoder &) = delete;
    MppDecoder &operator=(const MppDecoder &) = delete;

    /**
     * 初始化解码器
     * @param width 视频宽度
     * @param height 视频高度
     * @param coding_type 编码类型: MPP_VIDEO_CodingMJPEG, MPP_VIDEO_CodingAVC, MPP_VIDEO_CodingHEVC
     */
    bool init(
        uint32_t width,
        uint32_t height,
        MppCodingType coding_type = MPP_VIDEO_CodingMJPEG
    );
    void destroy();

    /**
     * 解码数据到 NV12
     * @param data 编码数据指针
     * @param size 编码数据大小
     * @param frame 输出解码帧
     * @return 成功返回 true
     */
    bool decode(
        const uint8_t *data,
        size_t size,
        DecodedFrame &frame
    );

private:
    void *m_impl;        // MppDecoderImpl*
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_hor_stride;
    uint32_t m_ver_stride;
    MppCodingType m_coding_type;
    bool m_initialized;
};

} // namespace player
