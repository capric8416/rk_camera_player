#pragma once

/**
 * FFmpegMuxer - FFmpeg MP4 容器封装器
 * 
 * 功能:
 * - 将 H.264/HEVC 编码数据封装为 MP4 容器
 * - 支持 fragmented MP4 (frag_keyframe + empty_moov)
 * - 支持 faststart (moov 前置)
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Forward declarations for FFmpeg types
struct AVFormatContext;
struct AVStream;
struct AVCodecParameters;
struct AVPacket;

namespace player {

/**
 * FFmpeg Muxer 配置
 */
struct MuxerConfig {
    std::string output_path;
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int bitrate = 4000000;          // bps
    bool use_hevc = true;           // true=HEVC, false=H264
    bool fragmented = true;         // frag_keyframe + empty_moov
    bool faststart = false;         // moov 前置 (与 fragmented 互斥)
    std::vector<uint8_t> extradata; // VPS/SPS/PPS
};

class FFmpegMuxer {
public:
    FFmpegMuxer();
    ~FFmpegMuxer();

    // 禁止拷贝
    FFmpegMuxer(const FFmpegMuxer&) = delete;
    FFmpegMuxer& operator=(const FFmpegMuxer&) = delete;

    /**
     * 打开输出文件
     * @param config 配置参数
     * @return 成功返回 true
     */
    bool open(const MuxerConfig& config);

    /**
     * 写入编码后的数据包
     * @param data 编码数据 (NAL units)
     * @param size 数据大小
     * @param is_keyframe 是否为关键帧
     * @param pts 显示时间戳 (帧序号)
     * @return 成功返回 true
     */
    bool write_packet(const uint8_t* data, size_t size, bool is_keyframe, int64_t pts);

    /**
     * 关闭文件并写入尾部
     * @return 成功返回 true
     */
    bool close();

    /**
     * 是否已打开
     */
    bool is_open() const { return m_opened; }

    /**
     * 获取已写入的帧数
     */
    int64_t frame_count() const { return m_frame_count; }

private:
    bool init_codec_params();
    
    AVFormatContext* m_fmt_ctx;
    AVStream* m_video_stream;
    AVPacket* m_packet;
    
    MuxerConfig m_config;
    bool m_opened;
    bool m_header_written;
    int64_t m_frame_count;
    int64_t m_time_base_num;
    int64_t m_time_base_den;
};

} // namespace player
