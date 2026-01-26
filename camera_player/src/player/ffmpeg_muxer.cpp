#include "ffmpeg_muxer.hpp"
#include "logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

namespace player {

FFmpegMuxer::FFmpegMuxer()
    : m_fmt_ctx(nullptr)
    , m_video_stream(nullptr)
    , m_packet(nullptr)
    , m_opened(false)
    , m_header_written(false)
    , m_frame_count(0)
    , m_time_base_num(1)
    , m_time_base_den(30)
{
}

FFmpegMuxer::~FFmpegMuxer() { close(); }

bool FFmpegMuxer::open(const MuxerConfig &config)
{
    if (m_opened) {
        LOG_WARN("Muxer already opened");
        return false;
    }

    m_config = config;
    m_time_base_den = config.fps;

    int ret;

    // 分配输出格式上下文
    ret = avformat_alloc_output_context2(
        &m_fmt_ctx,
        nullptr,
        "mp4",
        config.output_path.c_str()
    );
    if (ret < 0 || !m_fmt_ctx) {
        LOG_ERROR("avformat_alloc_output_context2 failed: {}", ret);
        return false;
    }

    // 创建视频流
    m_video_stream = avformat_new_stream(m_fmt_ctx, nullptr);
    if (!m_video_stream) {
        LOG_ERROR("avformat_new_stream failed");
        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
        return false;
    }

    // 设置编码参数
    if (!init_codec_params()) {
        LOG_ERROR("init_codec_params failed");
        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
        return false;
    }

    // 分配 packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        LOG_ERROR("av_packet_alloc failed");
        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
        return false;
    }

    // 打开输出文件
    if (!(m_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(
            &m_fmt_ctx->pb,
            config.output_path.c_str(),
            AVIO_FLAG_WRITE
        );
        if (ret < 0) {
            LOG_ERROR("avio_open failed: {}", ret);
            av_packet_free(&m_packet);
            avformat_free_context(m_fmt_ctx);
            m_fmt_ctx = nullptr;
            return false;
        }
    }

    // 设置 movflags
    AVDictionary *opts = nullptr;
    if (config.fragmented) {
        // Fragmented MP4: frag_keyframe + empty_moov + default_base_moof
        av_dict_set(
            &opts,
            "movflags",
            "frag_keyframe+empty_moov+default_base_moof",
            0
        );
        LOG_INFO("Using fragmented MP4 mode");
    } else if (config.faststart) {
        // Non-fragmented with faststart
        av_dict_set(&opts, "movflags", "faststart", 0);
        LOG_INFO("Using faststart MP4 mode");
    }

    // 写入文件头
    ret = avformat_write_header(m_fmt_ctx, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        LOG_ERROR("avformat_write_header failed: {}", ret);
        if (!(m_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_fmt_ctx->pb);
        }
        av_packet_free(&m_packet);
        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
        return false;
    }

    m_opened = true;
    m_header_written = true;
    m_frame_count = 0;

    LOG_INFO(
        "FFmpeg muxer opened: {} ({}x{} {}fps timebase={}/{} {})",
        config.output_path,
        config.width,
        config.height,
        config.fps,
        m_time_base_num,
        m_time_base_den,
        config.use_hevc ? "HEVC" : "H264"
    );

    return true;
}

bool FFmpegMuxer::init_codec_params()
{
    AVCodecParameters *par = m_video_stream->codecpar;

    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id = m_config.use_hevc ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    par->width = m_config.width;
    par->height = m_config.height;
    par->bit_rate = m_config.bitrate;
    par->format = AV_PIX_FMT_YUV420P; // 标记像素格式

    // 设置时间基
    m_video_stream->time_base = AVRational{1, m_config.fps};
    m_video_stream->avg_frame_rate = AVRational{m_config.fps, 1};

    // 设置 extradata (VPS/SPS/PPS)
    if (!m_config.extradata.empty()) {
        par->extradata_size = static_cast<int>(m_config.extradata.size());
        par->extradata = static_cast<uint8_t *>(
            av_mallocz(par->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE)
        );
        if (!par->extradata) {
            LOG_ERROR("Failed to allocate extradata");
            return false;
        }
        memcpy(par->extradata, m_config.extradata.data(), par->extradata_size);
    } else {
        LOG_WARN("No extradata provided for MP4 muxer!");
    }

    return true;
}

bool FFmpegMuxer::write_packet(
    const uint8_t *data,
    size_t size,
    bool is_keyframe,
    int64_t pts
)
{
    if (!m_opened || !m_fmt_ctx || !m_packet) {
        return false;
    }

    // 准备 packet
    av_packet_unref(m_packet);

    m_packet->data = const_cast<uint8_t *>(data);
    m_packet->size = static_cast<int>(size);
    m_packet->stream_index = m_video_stream->index;

    // 设置时间戳
    m_packet->pts = pts;
    m_packet->dts = pts;
    m_packet->duration = 1;

    // 转换时间基
    av_packet_rescale_ts(
        m_packet,
        AVRational{1, m_config.fps},
        m_video_stream->time_base
    );

    // 设置关键帧标志
    if (is_keyframe) {
        m_packet->flags |= AV_PKT_FLAG_KEY;
    }

    // 写入 packet
    int ret = av_interleaved_write_frame(m_fmt_ctx, m_packet);
    if (ret < 0) {
        LOG_ERROR("av_interleaved_write_frame failed: {}", ret);
        return false;
    }

    ++m_frame_count;
    return true;
}

bool FFmpegMuxer::close()
{
    if (!m_opened) {
        return true;
    }

    bool success = true;

    // 写入文件尾
    if (m_fmt_ctx && m_header_written) {
        int ret = av_write_trailer(m_fmt_ctx);
        if (ret < 0) {
            LOG_ERROR("av_write_trailer failed: {}", ret);
            success = false;
        }
    }

    // 关闭文件
    if (m_fmt_ctx && !(m_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_fmt_ctx->pb);
    }

    // 释放资源
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    if (m_fmt_ctx) {
        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
    }

    m_video_stream = nullptr;
    m_opened = false;
    m_header_written = false;

    LOG_INFO("FFmpeg muxer closed, {} frames written", m_frame_count);

    return success;
}

} // namespace player
