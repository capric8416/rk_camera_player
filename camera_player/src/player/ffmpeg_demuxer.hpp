#pragma once

/**
 * FFmpegDemuxer - FFmpeg 解封装器
 *
 * 功能:
 * - 打开视频文件 (支持 MP4 等容器)
 * - 读取视频流
 * - 自动进行 bitstream filter 转换 (MP4/AVCC -> Annex-B) 以适配 MPP 解码器
 */

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations for FFmpeg structs
struct AVFormatContext;
struct AVStream;
struct AVPacket;
struct AVBSFContext;

namespace player {

class FFmpegDemuxer {
public:
    FFmpegDemuxer();
    ~FFmpegDemuxer();

    // 禁止拷贝
    FFmpegDemuxer(const FFmpegDemuxer&) = delete;
    FFmpegDemuxer& operator=(const FFmpegDemuxer&) = delete;

    /**
     * 打开媒体文件
     * @param path 文件路径
     * @return 成功返回 true
     */
    bool open(const std::string& path);

    /**
     * 关闭文件
     */
    void close();

    /**
     * 读取下一个视频包 (自动转换为 Annex-B 格式)
     * @param data 输出数据缓冲区
     * @param pts 输出 PTS (微秒 or pure counter depending on stream, usually outputting user readable time is better but here pass through)
     * @return 成功读取返回 true，EOF 或错误返回 false
     */
    bool read_packet(std::vector<uint8_t>& data, int64_t& pts);

    /**
     * Seek 到指定时间 (秒)
     */
    bool seek(double seconds);

    // Getters
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    double get_fps() const { return m_fps; }
    bool is_hevc() const { return m_is_hevc; }
    int64_t get_duration_ms() const { return m_duration_ms; }

private:
    bool init_bsf();

    AVFormatContext* m_fmt_ctx;
    int m_video_stream_index;
    AVStream* m_video_stream;
    AVPacket* m_packet;
    AVBSFContext* m_bsf_ctx;  // Bitstream Filter Context

    int m_width;
    int m_height;
    double m_fps;
    bool m_is_hevc;
    int64_t m_duration_ms;
};

} // namespace player
