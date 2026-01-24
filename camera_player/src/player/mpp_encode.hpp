#pragma once

/**
 * MppEncoder - MPP H264/HEVC 编码器
 * 
 * 功能:
 * - 支持 H264/HEVC 编码
 * - 可配置 GOP, bitrate, fps, 码率控制方式
 * - 输出到文件
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <atomic>
#include <fstream>
#include <memory>
#include <vector>

#include "ffmpeg_muxer.hpp"

namespace player {

/**
 * 编码格式
 */
enum class EncoderCodec {
    H264,
    HEVC
};

/**
 * 像素格式
 */
enum class EncoderPixelFormat {
    NV12,
    NV21,
    YUV420P,
    RGB888,
    BGR888,
    ARGB8888
};

/**
 * 码率控制模式
 */
enum class RateControlMode {
    CBR,    // 固定码率
    VBR,    // 可变码率
    CQP     // 固定 QP
};

/**
 * 编码器配置
 */
struct EncoderConfig {
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t fps = 30;
    uint32_t gop = 300;             // I帧间隔
    uint32_t bitrate = 4000000;     // 比特率 (bps)
    EncoderCodec codec = EncoderCodec::HEVC;
    EncoderPixelFormat pix_fmt = EncoderPixelFormat::NV12;
    RateControlMode rc_mode = RateControlMode::CBR;
    std::string output_path;        // 输出文件路径
    
    // MP4 容器选项
    bool use_mp4_container = true;  // 使用 MP4 容器封装
    bool mp4_fragmented = true;     // frag_keyframe + empty_moov
    bool mp4_faststart = false;     // moov 前置 (与 fragmented 互斥)
};

/**
 * 输入帧数据
 */
struct InputFrame {
    void *data;
    int fd;                         // DMA-BUF fd, -1 表示使用 data 指针
    uint32_t size;
    uint32_t hor_stride;
    uint32_t ver_stride;
};

class MppEncoder {
public:
    MppEncoder();
    ~MppEncoder();

    // 禁止拷贝
    MppEncoder(const MppEncoder &) = delete;
    MppEncoder &operator=(const MppEncoder &) = delete;

    /**
     * 初始化编码器
     */
    bool init(const EncoderConfig &config);

    /**
     * 销毁编码器
     */
    void destroy();

    /**
     * 开始编码
     */
    bool start();

    /**
     * 停止编码
     */
    void stop();

    /**
     * 编码一帧
     * @param frame 输入帧数据
     * @return 成功返回 true
     */
    bool encode_frame(const InputFrame &frame);

    /**
     * 是否正在编码
     */
    bool is_encoding() const { return m_encoding; }

    /**
     * 获取已编码帧数
     */
    uint32_t frame_count() const { return m_frame_count; }

private:
    bool setup_encoder_config();
    bool write_packet_to_file(const void *data, size_t size, bool is_keyframe, int64_t pts);

    void *m_impl;                   // MppEncoderImpl*
    EncoderConfig m_config;
    std::atomic<bool> m_encoding;
    std::atomic<uint32_t> m_frame_count;
    bool m_initialized;
    std::unique_ptr<std::ofstream> m_output_file;
    std::unique_ptr<FFmpegMuxer> m_muxer;  // MP4 容器封装
    std::vector<uint8_t> m_header_data;    // VPS/SPS/PPS
};

} // namespace player
