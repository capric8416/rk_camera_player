#include "mpp_encode.hpp"
#include "logger.hpp"

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_meta.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi.h>

#include <cstring>

#ifndef MPP_ALIGN
#define MPP_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#endif

namespace player {

// 内部实现结构
struct MppEncoderImpl
{
    MppCtx ctx;
    MppApi *mpi;
    MppEncCfg cfg;

    MppBufferGroup buf_grp;
    MppBuffer frame_buf;
    MppFrame frame;

    uint32_t hor_stride;
    uint32_t ver_stride;
    size_t frame_size;
};

// 辅助函数：转换像素格式到 MPP 格式
static MppFrameFormat to_mpp_format(EncoderPixelFormat fmt)
{
    switch (fmt) {
    case EncoderPixelFormat::NV12:
        return MPP_FMT_YUV420SP;
    case EncoderPixelFormat::NV21:
        return MPP_FMT_YUV420SP_VU;
    case EncoderPixelFormat::YUV420P:
        return MPP_FMT_YUV420P;
    case EncoderPixelFormat::RGB888:
        return MPP_FMT_RGB888;
    case EncoderPixelFormat::BGR888:
        return MPP_FMT_BGR888;
    case EncoderPixelFormat::ARGB8888:
        return MPP_FMT_ARGB8888;
    default:
        return MPP_FMT_YUV420SP;
    }
}

// 辅助函数：转换编码格式到 MPP 格式
static MppCodingType to_mpp_codec(EncoderCodec codec)
{
    switch (codec) {
    case EncoderCodec::H264:
        return MPP_VIDEO_CodingAVC;
    case EncoderCodec::HEVC:
        return MPP_VIDEO_CodingHEVC;
    case EncoderCodec::MJPEG:
        return MPP_VIDEO_CodingMJPEG;
    default:
        return MPP_VIDEO_CodingAVC;
    }
}

MppEncoder::MppEncoder()
    : m_impl(nullptr)
    , m_encoding(false)
    , m_frame_count(0)
    , m_initialized(false)
    , m_output_file(nullptr)
    , m_muxer(nullptr)
{
}

MppEncoder::~MppEncoder()
{
    stop();
    destroy();
}

bool MppEncoder::init(const EncoderConfig &config)
{
    if (m_initialized) {
        LOG_WARN("编码器已初始化");
        return false;
    }

    if (config.width <= 0 || config.height <= 0 || config.width > 8192 ||
        config.height > 8192) {
        LOG_ERROR("无效的图像尺寸: {}x{}", config.width, config.height);
        return false;
    }

    m_config = config;

    auto *impl = new MppEncoderImpl();
    memset(impl, 0, sizeof(MppEncoderImpl));

    impl->hor_stride = MPP_ALIGN(config.width, 16);
    impl->ver_stride = MPP_ALIGN(config.height, 16);

    // 计算帧大小 (NV12 为 1.5 倍)
    impl->frame_size = impl->hor_stride * impl->ver_stride * 3 / 2;

    MPP_RET ret;

    // 创建 buffer group
    ret = mpp_buffer_group_get_internal(&impl->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_buffer_group_get_internal 失败");
        delete impl;
        return false;
    }

    // 分配 frame buffer
    ret = mpp_buffer_get(impl->buf_grp, &impl->frame_buf, impl->frame_size);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_buffer_get(frame) 失败");
        mpp_buffer_group_put(impl->buf_grp);
        delete impl;
        return false;
    }

    // 创建 frame
    ret = mpp_frame_init(&impl->frame);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_frame_init 失败");
        mpp_buffer_put(impl->frame_buf);
        mpp_buffer_group_put(impl->buf_grp);
        delete impl;
        return false;
    }

    // 设置 frame 属性
    mpp_frame_set_width(impl->frame, config.width);
    mpp_frame_set_height(impl->frame, config.height);
    mpp_frame_set_hor_stride(impl->frame, impl->hor_stride);
    mpp_frame_set_ver_stride(impl->frame, impl->ver_stride);
    mpp_frame_set_fmt(impl->frame, to_mpp_format(config.pix_fmt));
    mpp_frame_set_buffer(impl->frame, impl->frame_buf);

    // 创建 MPP 上下文
    ret = mpp_create(&impl->ctx, &impl->mpi);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_create 失败");
        mpp_frame_deinit(&impl->frame);
        mpp_buffer_put(impl->frame_buf);
        mpp_buffer_group_put(impl->buf_grp);
        delete impl;
        return false;
    }

    // 初始化编码器
    ret = mpp_init(impl->ctx, MPP_CTX_ENC, to_mpp_codec(config.codec));
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_init 失败");
        mpp_destroy(impl->ctx);
        mpp_frame_deinit(&impl->frame);
        mpp_buffer_put(impl->frame_buf);
        mpp_buffer_group_put(impl->buf_grp);
        delete impl;
        return false;
    }

    // 配置编码器
    mpp_enc_cfg_init(&impl->cfg);
    impl->mpi->control(impl->ctx, MPP_ENC_GET_CFG, impl->cfg);

    // 设置 m_impl 以便 setup_encoder_config 可以访问
    m_impl = impl;

    // 设置编码器配置
    if (!setup_encoder_config()) {
        LOG_ERROR("setup_encoder_config 失败");
        m_impl = nullptr;
        mpp_enc_cfg_deinit(impl->cfg);
        mpp_destroy(impl->ctx);
        mpp_frame_deinit(&impl->frame);
        mpp_buffer_put(impl->frame_buf);
        mpp_buffer_group_put(impl->buf_grp);
        delete impl;
        return false;
    }

    m_initialized = true;
    m_frame_count = 0;

    LOG_INFO(
        "MPP 编码器初始化: {}x{} stride={}x{} codec={} fps={} gop={} "
        "bitrate={}",
        config.width,
        config.height,
        impl->hor_stride,
        impl->ver_stride,
        config.codec == EncoderCodec::H264 ? "H264" : "HEVC",
        config.fps,
        config.gop,
        config.bitrate
    );

    return true;
}

bool MppEncoder::setup_encoder_config()
{
    if (!m_impl) {
        LOG_ERROR("setup_encoder_config: m_impl is null");
        return false;
    }

    auto *impl = static_cast<MppEncoderImpl *>(m_impl);

    LOG_DEBUG(
        "Configuring encoder: {}x{} stride={}x{} fps={} gop={} bitrate={}",
        m_config.width,
        m_config.height,
        impl->hor_stride,
        impl->ver_stride,
        m_config.fps,
        m_config.gop,
        m_config.bitrate
    );

    // 基本参数
    mpp_enc_cfg_set_s32(impl->cfg, "prep:width", m_config.width);
    mpp_enc_cfg_set_s32(impl->cfg, "prep:height", m_config.height);
    mpp_enc_cfg_set_s32(impl->cfg, "prep:hor_stride", impl->hor_stride);
    mpp_enc_cfg_set_s32(impl->cfg, "prep:ver_stride", impl->ver_stride);
    mpp_enc_cfg_set_s32(
        impl->cfg,
        "prep:format",
        to_mpp_format(m_config.pix_fmt)
    );

    // 码率控制 - RV1126 需要设置 quality 参数
    MppEncRcMode rc_mode = MPP_ENC_RC_MODE_CBR;
    switch (m_config.rc_mode) {
    case RateControlMode::CBR:
        rc_mode = MPP_ENC_RC_MODE_CBR;
        break;
    case RateControlMode::VBR:
        rc_mode = MPP_ENC_RC_MODE_VBR;
        break;
    case RateControlMode::CQP:
        rc_mode = MPP_ENC_RC_MODE_FIXQP;
        break;
    }
    mpp_enc_cfg_set_s32(impl->cfg, "rc:mode", rc_mode);

    // 码率
    mpp_enc_cfg_set_s32(impl->cfg, "rc:bps_target", m_config.bitrate);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:bps_max", m_config.bitrate * 17 / 16);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:bps_min", m_config.bitrate * 15 / 16);

    // 帧率
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_in_num", m_config.fps);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_out_flex", 0);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_out_num", m_config.fps);
    mpp_enc_cfg_set_s32(impl->cfg, "rc:fps_out_denorm", 1);

    // GOP
    mpp_enc_cfg_set_s32(impl->cfg, "rc:gop", m_config.gop);

    // 编码器特定配置
    if (m_config.codec == EncoderCodec::H264) {
        mpp_enc_cfg_set_s32(impl->cfg, "h264:profile", 100); // High profile
        mpp_enc_cfg_set_s32(impl->cfg, "h264:level", 40);    // Level 4.0
        mpp_enc_cfg_set_s32(impl->cfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:trans8x8", 1);
        // QP 范围
        mpp_enc_cfg_set_s32(impl->cfg, "h264:qp_init", 26);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:qp_max", 48);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:qp_min", 8);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:qp_max_i", 48);
        mpp_enc_cfg_set_s32(impl->cfg, "h264:qp_min_i", 8);
    } else if (m_config.codec == EncoderCodec::HEVC) {
        mpp_enc_cfg_set_s32(impl->cfg, "h265:qp_init", 26);
        mpp_enc_cfg_set_s32(impl->cfg, "h265:qp_max", 51);
        mpp_enc_cfg_set_s32(impl->cfg, "h265:qp_min", 10);
    } else if (m_config.codec == EncoderCodec::MJPEG) {
        // JPEG 质量 (1-99)
        mpp_enc_cfg_set_s32(impl->cfg, "jpeg:q_factor", 80);
        mpp_enc_cfg_set_s32(impl->cfg, "jpeg:qf_max", 99);
        mpp_enc_cfg_set_s32(impl->cfg, "jpeg:qf_min", 10);
    }

    // 应用配置
    MPP_RET ret = impl->mpi->control(impl->ctx, MPP_ENC_SET_CFG, impl->cfg);
    if (ret != MPP_OK) {
        LOG_ERROR("MPP_ENC_SET_CFG failed, ret = {}", static_cast<int>(ret));
        return false;
    }

    // 设置头部模式：在每个 IDR 帧前添加 SPS/PPS
    // 注意：即使设置了 EACH_IDR，MP4 封装仍通过 extradata 传递头部
    MppEncHeaderMode header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
    ret = impl->mpi->control(impl->ctx, MPP_ENC_SET_HEADER_MODE, &header_mode);
    if (ret != MPP_OK) {
        LOG_WARN(
            "MPP_ENC_SET_HEADER_MODE failed, ret = {}",
            static_cast<int>(ret)
        );
    } else {
        LOG_INFO("Header mode set to EACH_IDR");
    }

    // 获取 extradata (VPS/SPS/PPS)
    MppPacket packet = nullptr;
    ret = impl->mpi->control(impl->ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
    if (ret == MPP_OK && packet) {
        void *ptr = mpp_packet_get_pos(packet);
        size_t len = mpp_packet_get_length(packet);

        if (ptr && len > 0) {
            m_header_data.resize(len);
            memcpy(m_header_data.data(), ptr, len);
            LOG_INFO("Captured codec extradata: {} bytes", len);
        }

        mpp_packet_deinit(&packet);
    } else {
        LOG_WARN("Failed to get codec extra info, MP4 muxing might fail");
    }

    return true;
}

void MppEncoder::destroy()
{
    if (!m_impl)
        return;

    auto *impl = static_cast<MppEncoderImpl *>(m_impl);

    if (impl->mpi && impl->ctx) {
        impl->mpi->reset(impl->ctx);
    }

    if (impl->frame) {
        mpp_frame_deinit(&impl->frame);
    }

    if (impl->cfg) {
        mpp_enc_cfg_deinit(impl->cfg);
    }

    if (impl->ctx) {
        mpp_destroy(impl->ctx);
    }

    if (impl->frame_buf) {
        mpp_buffer_put(impl->frame_buf);
    }

    if (impl->buf_grp) {
        mpp_buffer_group_put(impl->buf_grp);
    }

    delete impl;
    m_impl = nullptr;
    m_initialized = false;

    LOG_INFO("MPP 编码器已销毁");
}

bool MppEncoder::start()
{
    if (!m_initialized) {
        LOG_ERROR("编码器未初始化");
        return false;
    }

    if (m_encoding) {
        LOG_WARN("编码器已在运行");
        return true;
    }

    // 打开输出
    if (!m_config.output_path.empty()) {
        // MJPEG 通常不使用 MP4 容器，除非是 MJPEG-in-MP4，这里默认 JPEG
        // 直接输出
        if (m_config.use_mp4_container &&
            m_config.codec != EncoderCodec::MJPEG) {
            // 使用 FFmpeg muxer 封装为 MP4
            m_muxer = std::make_unique<FFmpegMuxer>();

            MuxerConfig mux_cfg;
            mux_cfg.output_path = m_config.output_path;
            mux_cfg.width = m_config.width;
            mux_cfg.height = m_config.height;
            mux_cfg.fps = m_config.fps;
            mux_cfg.bitrate = m_config.bitrate;
            mux_cfg.use_hevc = (m_config.codec == EncoderCodec::HEVC);
            mux_cfg.fragmented = m_config.mp4_fragmented;
            mux_cfg.faststart = m_config.mp4_faststart;

            // 传递 extradata
            if (!m_header_data.empty()) {
                mux_cfg.extradata = m_header_data;
            } else {
                LOG_WARN(
                    "Starting MP4 muxer without extradata (headers may be "
                    "missing)"
                );
            }

            if (!m_muxer->open(mux_cfg)) {
                LOG_ERROR("无法打开 MP4 muxer: {}", m_config.output_path);
                m_muxer.reset();
                return false;
            }
        } else {
            // 原始流输出
            m_output_file = std::make_unique<std::ofstream>(
                m_config.output_path,
                std::ios::binary
            );
            if (!m_output_file->is_open()) {
                LOG_ERROR("无法打开输出文件: {}", m_config.output_path);
                m_output_file.reset();
                return false;
            }

            // 如果是 H264/H265 原始流，需要先写入序列头部 (SPS/PPS/VPS)
            if (!m_header_data.empty()) {
                m_output_file->write(
                    reinterpret_cast<const char *>(m_header_data.data()),
                    m_header_data.size()
                );
                LOG_INFO("写入原始流序列头: {} 字节", m_header_data.size());
            }
        }
    }

    m_encoding = true;
    m_frame_count = 0;
    LOG_INFO(
        "编码器已启动, 输出: {} ({})",
        m_config.output_path,
        m_config.use_mp4_container ? "MP4" : "RAW"
    );
    return true;
}

void MppEncoder::stop()
{
    if (!m_encoding)
        return;

    m_encoding = false;

    // 停止前排干编码器中的剩余包 (Drain)
    if (m_initialized && m_impl) {
        auto *impl = static_cast<MppEncoderImpl *>(m_impl);
        MppPacket packet = nullptr;
        // 使用非阻塞或短超时获取最后几帧
        while (impl->mpi->encode_get_packet(impl->ctx, &packet) == MPP_OK &&
               packet) {
            void *ptr = mpp_packet_get_pos(packet);
            size_t len = mpp_packet_get_length(packet);
            if (ptr && len > 0) {
                RK_U32 is_intra = 0;
                MppMeta meta = mpp_packet_get_meta(packet);
                if (meta) {
                    mpp_meta_get_s32(
                        meta,
                        KEY_OUTPUT_INTRA,
                        reinterpret_cast<RK_S32 *>(&is_intra)
                    );
                }
                write_packet_to_file(ptr, len, is_intra != 0, m_frame_count++);
            }
            mpp_packet_deinit(&packet);
        }
    }

    if (m_muxer) {
        m_muxer->close();
        m_muxer.reset();
    }

    if (m_output_file) {
        m_output_file->close();
        m_output_file.reset();
    }

    LOG_INFO("编码器已停止, 共编码 {} 帧", m_frame_count.load());
}

bool MppEncoder::encode_frame(const InputFrame &frame)
{
    if (!m_initialized || !m_encoding) {
        return false;
    }

    if (frame.fd < 0) {
        if (!frame.data || frame.size == 0) {
            LOG_ERROR(
                "MppEncoder::encode_frame: frame.data 是空指针或 size 为 0 (fd "
                "= -1)"
            );
            return false;
        }
    }

    auto *impl = static_cast<MppEncoderImpl *>(m_impl);

    // 检查 frame.size 是否超过了分配的 buffer 大小
    if (frame.fd < 0 && frame.size > impl->frame_size) {
        LOG_ERROR(
            "MppEncoder::encode_frame: 数据大小 ({}) 超过了缓冲区最大限制 ({})",
            frame.size,
            impl->frame_size
        );
        return false;
    }

    MPP_RET ret;

    // 设置输入帧数据
    if (frame.fd >= 0) {
        // 使用 DMA-BUF fd
        MppBufferInfo info;
        memset(&info, 0, sizeof(info));
        info.type = MPP_BUFFER_TYPE_DRM;
        info.fd = frame.fd;
        info.size = frame.size;
        info.ptr = frame.data;

        MppBuffer buf = nullptr;
        ret = mpp_buffer_import(&buf, &info);
        if (ret == MPP_OK && buf) {
            mpp_frame_set_buffer(impl->frame, buf);
            mpp_buffer_put(buf);
        }
    } else {
        // 复制数据到预分配的 buffer
        void *dst = mpp_buffer_get_ptr(impl->frame_buf);
        memcpy(dst, frame.data, frame.size);
        mpp_frame_set_buffer(impl->frame, impl->frame_buf);
    }

    mpp_frame_set_hor_stride(
        impl->frame,
        frame.hor_stride ? frame.hor_stride : impl->hor_stride
    );
    mpp_frame_set_ver_stride(
        impl->frame,
        frame.ver_stride ? frame.ver_stride : impl->ver_stride
    );

    // 设置时间戳，防止率控失效
    mpp_frame_set_pts(impl->frame, m_frame_count);

    // 使用同步编码接口
    MppPacket packet = nullptr;
    ret = impl->mpi->encode_put_frame(impl->ctx, impl->frame);
    if (ret != MPP_OK) {
        LOG_ERROR("encode_put_frame 失败: {}", static_cast<int>(ret));
        return false;
    }

    ret = impl->mpi->encode_get_packet(impl->ctx, &packet);
    if (ret != MPP_OK) {
        LOG_ERROR("encode_get_packet 失败: {}", static_cast<int>(ret));
        return false;
    }

    // 获取编码数据
    if (packet) {
        void *ptr = mpp_packet_get_pos(packet);
        size_t len = mpp_packet_get_length(packet);

        if (ptr && len > 0) {
            // 检测关键帧
            RK_U32 is_intra = 0;
            MppMeta meta = mpp_packet_get_meta(packet);
            if (meta) {
                mpp_meta_get_s32(
                    meta,
                    KEY_OUTPUT_INTRA,
                    reinterpret_cast<RK_S32 *>(&is_intra)
                );
            }
            bool is_keyframe = (is_intra != 0);

            write_packet_to_file(ptr, len, is_keyframe, m_frame_count);
            ++m_frame_count;
        }

        mpp_packet_deinit(&packet);
    }

    return true;
}

bool MppEncoder::write_packet_to_file(
    const void *data,
    size_t size,
    bool is_keyframe,
    int64_t pts
)
{
    if (!data || size == 0) {
        return false;
    }

    // 使用 MP4 muxer
    if (m_muxer && m_muxer->is_open()) {
        return m_muxer->write_packet(
            static_cast<const uint8_t *>(data),
            size,
            is_keyframe,
            pts
        );
    }

    // 原始流输出
    if (m_output_file && m_output_file->is_open()) {
        m_output_file->write(static_cast<const char *>(data), size);
        return m_output_file->good();
    }

    return false;
}

} // namespace player
