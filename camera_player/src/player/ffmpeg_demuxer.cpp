#include "ffmpeg_demuxer.hpp"
#include "logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

namespace player {

FFmpegDemuxer::FFmpegDemuxer()
    : m_fmt_ctx(nullptr)
    , m_video_stream_index(-1)
    , m_video_stream(nullptr)
    , m_packet(nullptr)
    , m_bsf_ctx(nullptr)
    , m_width(0)
    , m_height(0)
    , m_fps(0.0)
    , m_is_hevc(false)
    , m_duration_ms(0)
{
}

FFmpegDemuxer::~FFmpegDemuxer() { close(); }

bool FFmpegDemuxer::open(const std::string &path)
{
    if (m_fmt_ctx) {
        LOG_WARN("Demuxer already opened");
        return false;
    }

    int ret = 0;

    // 打开输入文件
    ret = avformat_open_input(&m_fmt_ctx, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("avformat_open_input failed: {} ({})", path, ret);
        return false;
    }

    // 查找流信息
    ret = avformat_find_stream_info(m_fmt_ctx, nullptr);
    if (ret < 0) {
        LOG_ERROR("avformat_find_stream_info failed: {}", ret);
        close();
        return false;
    }

    // 查找视频流
    m_video_stream_index = -1;
    for (unsigned int i = 0; i < m_fmt_ctx->nb_streams; ++i) {
        if (m_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_video_stream_index = i;
            m_video_stream = m_fmt_ctx->streams[i];
            break;
        }
    }

    if (m_video_stream_index < 0 || !m_video_stream) {
        LOG_ERROR("No video stream found in {}", path);
        close();
        return false;
    }

    AVCodecParameters *param = m_video_stream->codecpar;

    // 获取视频信息
    m_width = param->width;
    m_height = param->height;
    if (m_video_stream->avg_frame_rate.den > 0) {
        m_fps = av_q2d(m_video_stream->avg_frame_rate);
    } else {
        m_fps = 30.0; // 默认
    }

    // 计算时长 (ms)
    if (m_video_stream->duration != AV_NOPTS_VALUE) {
        m_duration_ms = static_cast<int64_t>(
            m_video_stream->duration * av_q2d(m_video_stream->time_base) * 1000
        );
    } else if (m_fmt_ctx->duration != AV_NOPTS_VALUE) {
        m_duration_ms = m_fmt_ctx->duration / 1000;
    }

    // 判断编码格式
    if (param->codec_id == AV_CODEC_ID_HEVC) {
        m_is_hevc = true;
    } else if (param->codec_id == AV_CODEC_ID_H264) {
        m_is_hevc = false;
    } else {
        LOG_ERROR(
            "Unsupported codec ID: {}",
            static_cast<int>(param->codec_id)
        );
        close();
        return false;
    }

    // 初始化 Bitstream Filter (MP4 -> Annex-B)
    if (!init_bsf()) {
        LOG_ERROR("Failed to init bitstream filter");
        close();
        return false;
    }

    m_packet = av_packet_alloc();
    if (!m_packet) {
        LOG_ERROR("av_packet_alloc failed");
        close();
        return false;
    }

    LOG_INFO(
        "Demuxer opened: {} ({}x{} {:.2f}fps {})",
        path,
        m_width,
        m_height,
        m_fps,
        m_is_hevc ? "HEVC" : "H264"
    );

    return true;
}

bool FFmpegDemuxer::init_bsf()
{
    const char *filter_name =
        m_is_hevc ? "hevc_mp4toannexb" : "h264_mp4toannexb";
    const AVBitStreamFilter *bsf = av_bsf_get_by_name(filter_name);
    if (!bsf) {
        LOG_ERROR("Could not find bitstream filter: {}", filter_name);
        return false;
    }

    int ret = av_bsf_alloc(bsf, &m_bsf_ctx);
    if (ret < 0) {
        LOG_ERROR("av_bsf_alloc failed: {}", ret);
        return false;
    }

    // 复制编码参数到 BSF
    ret = avcodec_parameters_copy(m_bsf_ctx->par_in, m_video_stream->codecpar);
    if (ret < 0) {
        LOG_ERROR("avcodec_parameters_copy failed: {}", ret);
        return false;
    }

    ret = av_bsf_init(m_bsf_ctx);
    if (ret < 0) {
        LOG_ERROR("av_bsf_init failed: {}", ret);
        return false;
    }

    return true;
}

void FFmpegDemuxer::close()
{
    if (m_bsf_ctx) {
        av_bsf_free(&m_bsf_ctx);
        m_bsf_ctx = nullptr;
    }

    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    if (m_fmt_ctx) {
        avformat_close_input(&m_fmt_ctx);
        m_fmt_ctx = nullptr;
    }

    m_video_stream = nullptr;
    m_video_stream_index = -1;
}

bool FFmpegDemuxer::seek(double seconds)
{
    if (!m_fmt_ctx || m_video_stream_index < 0 || !m_bsf_ctx) {
        LOG_ERROR("Demuxer not opened or initialized");
        return false;
    }

    int64_t timestamp =
        static_cast<int64_t>(seconds / av_q2d(m_video_stream->time_base));
    int ret = av_seek_frame(
        m_fmt_ctx,
        m_video_stream_index,
        timestamp,
        AVSEEK_FLAG_BACKWARD
    );
    if (ret < 0) {
        LOG_WARN("Seek failed: {}", ret);
        return false;
    }

    // Seek 后需要 flush BSF
    if (m_bsf_ctx) {
        av_bsf_flush(m_bsf_ctx);
    }

    return true;
}

bool FFmpegDemuxer::read_packet(std::vector<uint8_t> &data, int64_t &pts)
{
    if (!m_fmt_ctx || !m_video_stream || !m_bsf_ctx || !m_packet) {
        LOG_ERROR("Demuxer not opened or initialized");
        return false;
    }

    data.clear();
    int ret;

    while (true) {
        // 1. 尝试从 BSF 获取处理后的包
        ret = av_bsf_receive_packet(m_bsf_ctx, m_packet);
        if (ret == 0) {
            // 成功获取
            data.assign(m_packet->data, m_packet->data + m_packet->size);
            pts = m_packet->pts; // 这里保留原始 TimeBase 的 pts

            // LOG_TRACE("Read packet: size={}, pts={}", data.size(), pts);
            av_packet_unref(m_packet);
            return true;
        }

        if (ret != AVERROR(EAGAIN)) {
            // BSF 错误或 EOF（如果是 EOF 且 drain 完毕）
            if (ret == AVERROR_EOF) {
                // 如果 BSF 输出完毕，我们还需要检查文件是否还有新数据
                // 但通常 loop 会在 av_read_frame 返回 EOF 后发送 null packet 到
                // BSF 导致 BSF 最终也返回 EOF LOG_DEBUG("av_bsf_receive_packet
                // EOF");
                return false;
            }
            LOG_ERROR("av_bsf_receive_packet error: {}", ret);
            return false;
        }

        // 2. 需要更多输入数据 (EAGAIN)
        // 从文件读取
        ret = av_read_frame(m_fmt_ctx, m_packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // 文件结束，通过发送 NULL packet 刷新 BSF
                av_bsf_send_packet(m_bsf_ctx, nullptr);
                LOG_DEBUG("av_read_frame EOF, flushing BSF");
                // 继续循环以获取 BSF 中剩余的数据
                continue;
            }
            LOG_ERROR("av_read_frame error: {}", ret);
            return false;
        }

        // 检查是不是视频流
        if (m_packet->stream_index == m_video_stream_index) {
            // 发送到 BSF
            ret = av_bsf_send_packet(m_bsf_ctx, m_packet);
            if (ret < 0) {
                LOG_ERROR("av_bsf_send_packet failed: {}", ret);
                av_packet_unref(m_packet);
                return false;
            }
        } else {
            // LOG_DEBUG("Skipping non-video packet stream={}",
            // m_packet->stream_index);
        }

        // 无论是否发送成功（或非视频包），都要释放原始包
        av_packet_unref(m_packet);
    }
}

} // namespace player
