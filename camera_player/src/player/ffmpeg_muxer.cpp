#include "ffmpeg_muxer.hpp"
#include "logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

namespace player {

FFmpegMuxer::FFmpegMuxer()
    : m_fmt_ctx(nullptr)
    , m_video_stream(nullptr)
    , m_packet(nullptr)
    , m_bsf_ctx(nullptr)
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

    // 初始化 bitstream filter 以提取 extradata (SPS/PPS) 供 MP4 封装
    // 因为 MPP 输出的是 Annex-B (包含 SPS/PPS 的带 start code 码流)，
    // 而 MP4 容器格式本身不携带 in-band SPS/PPS，而是放在 AVCC/HVCC box 中。
    // extract_extradata 会把 in-band 的 SPS/PPS 提取出来，
    // 并且 FFmpeg 写入 MP4 时如果它能拿到完整的 context，会自动把 packet 里的
    // 00000001 start code 替换为 4字节的 length prefix。
    const char *bsf_name = "extract_extradata";
    const AVBitStreamFilter *bsf = av_bsf_get_by_name(bsf_name);
    if (bsf) {
        ret = av_bsf_alloc(bsf, &m_bsf_ctx);
        if (ret >= 0) {
            avcodec_parameters_copy(
                m_bsf_ctx->par_in,
                m_video_stream->codecpar
            );
            ret = av_bsf_init(m_bsf_ctx);
            if (ret < 0) {
                LOG_WARN("Failed to init bitstream filter {}", bsf_name);
                av_bsf_free(&m_bsf_ctx);
            }
        }
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
        av_bsf_free(&m_bsf_ctx);
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
    // 如果 extradata 非空，FFmpeg 的 mp4 muxer 会在写入 header 时解析它
    // 来生成 avcC/hvcC (AVCDecoderConfigurationRecord)。
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

    if (!data || size == 0) {
        LOG_ERROR("write_packet: invalid data (null) or size (0)");
        return false;
    }

    // Prepare packet
    av_packet_unref(m_packet);
    m_packet->data = const_cast<uint8_t *>(data);
    m_packet->size = static_cast<int>(size);
    m_packet->stream_index = m_video_stream->index;
    m_packet->pts = pts;
    m_packet->dts = pts;
    m_packet->duration = 1; // Assuming 1 frame duration

    // Set keyframe flag
    if (is_keyframe) {
        m_packet->flags |= AV_PKT_FLAG_KEY;
    }

    // Rescale timestamps before filtering
    av_packet_rescale_ts(
        m_packet,
        AVRational{1, m_config.fps},
        m_video_stream->time_base
    );

    if (m_bsf_ctx) {
        // Apply bitstream filter
        int bsf_ret = av_bsf_send_packet(m_bsf_ctx, m_packet);
        if (bsf_ret < 0) {
            LOG_ERROR("av_bsf_send_packet failed: {}", bsf_ret);
            return false;
        }

        while ((bsf_ret = av_bsf_receive_packet(m_bsf_ctx, m_packet)) == 0) {
            // Write the filtered packet
            bsf_ret = av_interleaved_write_frame(m_fmt_ctx, m_packet);
            if (bsf_ret < 0) {
                LOG_ERROR(
                    "av_interleaved_write_frame failed after bsf: {}",
                    bsf_ret
                );
                av_packet_unref(m_packet);
                return false;
            }
            av_packet_unref(m_packet);
            ++m_frame_count;
        }

        if (bsf_ret != AVERROR(EAGAIN) && bsf_ret != AVERROR_EOF) {
            LOG_ERROR("av_bsf_receive_packet failed: {}", bsf_ret);
            return false;
        }
    } else {
        // 直接写入
        int ret = av_interleaved_write_frame(m_fmt_ctx, m_packet);
        if (ret < 0) {
            LOG_ERROR("av_interleaved_write_frame failed: {}", ret);
            return false;
        }
        ++m_frame_count;
    }

    return true;
}

bool FFmpegMuxer::close()
{
    if (!m_opened) {
        return true;
    }

    bool success = true;

    // Flush the bitstream filter
    if (m_bsf_ctx) {
        av_bsf_send_packet(m_bsf_ctx, nullptr); // Send null packet to flush
        int ret;
        while ((ret = av_bsf_receive_packet(m_bsf_ctx, m_packet)) == 0) {
            // Write any remaining filtered packets
            ret = av_interleaved_write_frame(m_fmt_ctx, m_packet);
            if (ret < 0) {
                LOG_ERROR(
                    "av_interleaved_write_frame failed during bsf flush: {}",
                    ret
                );
                success = false;
            }
            av_packet_unref(m_packet);
        }
        if (ret != AVERROR_EOF) {
            LOG_ERROR("av_bsf_receive_packet failed during flush: {}", ret);
            success = false;
        }
    }

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

    if (m_bsf_ctx) {
        av_bsf_free(&m_bsf_ctx);
        m_bsf_ctx = nullptr;
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
