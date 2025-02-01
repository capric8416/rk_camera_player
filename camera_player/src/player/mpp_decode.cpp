#include "mpp_decode.hpp"
#include "logger.hpp"

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_meta.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_task.h>
#include <rockchip/rk_mpi.h>

#include <cstring>

#ifndef MPP_ALIGN
#define MPP_ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))
#endif

namespace player {

// 内部实现结构
struct MppDecoderImpl
{
    MppCtx ctx;
    MppApi *mpi;
    MppDecCfg cfg;

    // Task mode 资源 (MJPEG)
    MppBufferGroup frame_grp;
    MppBufferGroup packet_grp;
    MppBuffer frame_buf;
    MppBuffer packet_buf;
    MppFrame frame;
    MppPacket packet;
    size_t packet_size;

    // Simple mode 资源 (H.264/H.265)
    void *simple_packet_buf;
    size_t simple_packet_buf_size;
    MppFrame simple_frame; // 保存帧引用

    bool use_simple_api; // true 使用简单 API, false 使用 task mode
};

// 转换 coding_type 为字符串
static const char *coding_type_to_string(MppCodingType type)
{
    switch (type) {
    case MPP_VIDEO_CodingMJPEG:
        return "MJPEG";
    case MPP_VIDEO_CodingAVC:
        return "H.264";
    case MPP_VIDEO_CodingHEVC:
        return "H.265";
    default:
        return "Unknown";
    }
}

MppDecoder::MppDecoder()
    : m_impl(nullptr)
    , m_width(0)
    , m_height(0)
    , m_hor_stride(0)
    , m_ver_stride(0)
    , m_coding_type(MPP_VIDEO_CodingMJPEG)
    , m_initialized(false)
{
}

MppDecoder::~MppDecoder() { destroy(); }

bool MppDecoder::init(
    uint32_t width,
    uint32_t height,
    MppCodingType coding_type
)
{
    m_width = width;
    m_height = height;
    m_coding_type = coding_type;
    m_hor_stride = MPP_ALIGN(width, 16);
    m_ver_stride = MPP_ALIGN(height, 16);

    auto *impl = new MppDecoderImpl();
    memset(impl, 0, sizeof(MppDecoderImpl));

    // H.264/H.265 使用简单 API，MJPEG 使用 task mode
    impl->use_simple_api =
        (coding_type == MPP_VIDEO_CodingAVC ||
         coding_type == MPP_VIDEO_CodingHEVC);

    MPP_RET ret;

    if (impl->use_simple_api) {
        // ========== Simple API 模式 (H.264/H.265) ==========
        impl->simple_packet_buf_size = width * height * 2;
        impl->simple_packet_buf = malloc(impl->simple_packet_buf_size);
        if (!impl->simple_packet_buf) {
            LOG_ERROR("分配 packet 缓冲区失败");
            delete impl;
            return false;
        }
    } else {
        // ========== Task Mode API (MJPEG) ==========
        impl->packet_size = width * height;

        // 创建 buffer groups
        ret = mpp_buffer_group_get_internal(
            &impl->frame_grp,
            MPP_BUFFER_TYPE_DRM
        );
        if (ret != MPP_OK) {
            LOG_ERROR("mpp_buffer_group_get_internal(frame) 失败");
            delete impl;
            return false;
        }

        ret = mpp_buffer_group_get_internal(
            &impl->packet_grp,
            MPP_BUFFER_TYPE_DRM
        );
        if (ret != MPP_OK) {
            LOG_ERROR("mpp_buffer_group_get_internal(packet) 失败");
            mpp_buffer_group_put(impl->frame_grp);
            delete impl;
            return false;
        }

        // 创建 frame
        ret = mpp_frame_init(&impl->frame);
        if (ret != MPP_OK) {
            LOG_ERROR("mpp_frame_init 失败");
            mpp_buffer_group_put(impl->packet_grp);
            mpp_buffer_group_put(impl->frame_grp);
            delete impl;
            return false;
        }

        // 分配 packet buffer
        ret = mpp_buffer_get(
            impl->packet_grp,
            &impl->packet_buf,
            impl->packet_size + 1024
        );
        if (ret != MPP_OK) {
            LOG_ERROR("mpp_buffer_get(packet) 失败");
            mpp_frame_deinit(&impl->frame);
            mpp_buffer_group_put(impl->packet_grp);
            mpp_buffer_group_put(impl->frame_grp);
            delete impl;
            return false;
        }

        // 创建 packet
        mpp_packet_init_with_buffer(&impl->packet, impl->packet_buf);

        // 分配 frame buffer
        size_t frame_size = m_hor_stride * m_ver_stride * 4;
        ret = mpp_buffer_get(impl->frame_grp, &impl->frame_buf, frame_size);
        if (ret != MPP_OK) {
            LOG_ERROR("mpp_buffer_get(frame) 失败");
            mpp_packet_deinit(&impl->packet);
            mpp_buffer_put(impl->packet_buf);
            mpp_buffer_group_put(impl->packet_grp);
            mpp_buffer_group_put(impl->frame_grp);
            delete impl;
            return false;
        }
        mpp_frame_set_buffer(impl->frame, impl->frame_buf);
    }

    // 创建 MPP 上下文
    ret = mpp_create(&impl->ctx, &impl->mpi);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_create 失败");
        if (impl->use_simple_api) {
            free(impl->simple_packet_buf);
        } else {
            mpp_buffer_put(impl->frame_buf);
            mpp_packet_deinit(&impl->packet);
            mpp_buffer_put(impl->packet_buf);
            mpp_buffer_group_put(impl->packet_grp);
            mpp_buffer_group_put(impl->frame_grp);
        }
        delete impl;
        return false;
    }

    // 初始化解码器
    ret = mpp_init(impl->ctx, MPP_CTX_DEC, coding_type);
    if (ret != MPP_OK) {
        LOG_ERROR(
            "mpp_init 失败, coding_type={}",
            coding_type_to_string(coding_type)
        );
        mpp_destroy(impl->ctx);
        if (impl->use_simple_api) {
            free(impl->simple_packet_buf);
        } else {
            mpp_buffer_put(impl->frame_buf);
            mpp_packet_deinit(&impl->packet);
            mpp_buffer_put(impl->packet_buf);
            mpp_buffer_group_put(impl->packet_grp);
            mpp_buffer_group_put(impl->frame_grp);
        }
        delete impl;
        return false;
    }

    // 配置解码器
    mpp_dec_cfg_init(&impl->cfg);
    impl->mpi->control(impl->ctx, MPP_DEC_GET_CFG, impl->cfg);

    // FFmpeg Demuxer (配合 bitstream filter) 已经输出了完整的 Annex-B 帧
    // 所以不需要开启 MPP 的 split_parse (裸流分包模式)
    if (impl->use_simple_api) {
        mpp_dec_cfg_set_u32(impl->cfg, "base:split_parse", 0);
    }
    impl->mpi->control(impl->ctx, MPP_DEC_SET_CFG, impl->cfg);

    m_impl = impl;
    m_initialized = true;

    LOG_INFO(
        "MPP 解码器初始化: {}x{} stride={}x{} codec={} mode={}",
        m_width,
        m_height,
        m_hor_stride,
        m_ver_stride,
        coding_type_to_string(m_coding_type),
        impl->use_simple_api ? "simple" : "task"
    );
    return true;
}

void MppDecoder::destroy()
{
    if (!m_impl) {
        return;
    }

    auto *impl = static_cast<MppDecoderImpl *>(m_impl);

    if (impl->mpi && impl->ctx) {
        impl->mpi->reset(impl->ctx);
    }

    if (impl->cfg) {
        mpp_dec_cfg_deinit(impl->cfg);
    }

    if (impl->use_simple_api) {
        if (impl->simple_frame) {
            mpp_frame_deinit(&impl->simple_frame);
        }
        if (impl->simple_packet_buf) {
            free(impl->simple_packet_buf);
        }
    } else {
        if (impl->packet) {
            mpp_packet_deinit(&impl->packet);
        }
        if (impl->frame) {
            mpp_frame_deinit(&impl->frame);
        }
        if (impl->frame_buf) {
            mpp_buffer_put(impl->frame_buf);
        }
        if (impl->packet_buf) {
            mpp_buffer_put(impl->packet_buf);
        }
        if (impl->packet_grp) {
            mpp_buffer_group_put(impl->packet_grp);
        }
        if (impl->frame_grp) {
            mpp_buffer_group_put(impl->frame_grp);
        }
    }

    if (impl->ctx) {
        mpp_destroy(impl->ctx);
    }

    delete impl;
    m_impl = nullptr;
    m_initialized = false;
}

// Task Mode 解码 (MJPEG)
static bool decode_task_mode(
    MppDecoderImpl *impl,
    const uint8_t *data,
    size_t size,
    DecodedFrame &frame
)
{
    // 复制数据到 packet buffer
    memcpy(mpp_packet_get_data(impl->packet), data, size);
    mpp_packet_set_length(impl->packet, size);
    mpp_packet_set_pos(impl->packet, mpp_packet_get_data(impl->packet));

    MPP_RET ret;
    MppTask task = nullptr;

    // 等待输入端口就绪
    ret = impl->mpi->poll(impl->ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        LOG_ERROR("poll INPUT 失败");
        return false;
    }

    // 从输入端口取出任务
    ret = impl->mpi->dequeue(impl->ctx, MPP_PORT_INPUT, &task);
    if (ret != MPP_OK || !task) {
        LOG_ERROR("dequeue INPUT 失败");
        return false;
    }

    // 设置任务的输入 packet 和输出 frame
    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, impl->packet);
    mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, impl->frame);

    // 提交任务
    ret = impl->mpi->enqueue(impl->ctx, MPP_PORT_INPUT, task);
    if (ret != MPP_OK) {
        LOG_ERROR("enqueue INPUT 失败");
        return false;
    }

    // 等待输出
    ret = impl->mpi->poll(impl->ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        LOG_ERROR("poll OUTPUT 失败");
        return false;
    }

    task = nullptr;
    ret = impl->mpi->dequeue(impl->ctx, MPP_PORT_OUTPUT, &task);
    if (ret != MPP_OK || !task) {
        LOG_ERROR("dequeue OUTPUT 失败");
        return false;
    }

    // 获取解码后的 frame
    MppFrame mpp_frame = nullptr;
    mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &mpp_frame);

    // 归还任务
    impl->mpi->enqueue(impl->ctx, MPP_PORT_OUTPUT, task);

    if (!mpp_frame) {
        LOG_ERROR("获取帧失败");
        return false;
    }

    // 获取帧缓冲区
    MppBuffer buffer = mpp_frame_get_buffer(mpp_frame);
    if (!buffer) {
        LOG_ERROR("获取帧缓冲区失败");
        return false;
    }

    frame.data = mpp_buffer_get_ptr(buffer);
    frame.fd = mpp_buffer_get_fd(buffer);
    frame.width = mpp_frame_get_width(mpp_frame);
    frame.height = mpp_frame_get_height(mpp_frame);
    frame.hor_stride = mpp_frame_get_hor_stride(mpp_frame);
    frame.ver_stride = mpp_frame_get_ver_stride(mpp_frame);

    return true;
}

// Simple API 解码 (H.264/H.265)
static bool decode_simple_mode(
    MppDecoderImpl *impl,
    const uint8_t *data,
    size_t size,
    DecodedFrame &frame,
    uint32_t &out_width,
    uint32_t &out_height,
    uint32_t &out_hor_stride,
    uint32_t &out_ver_stride
)
{
    if (size > impl->simple_packet_buf_size) {
        LOG_WARN("NAL 数据过大: {} > {}", size, impl->simple_packet_buf_size);
        return false;
    }

    // 复制数据
    memcpy(impl->simple_packet_buf, data, size);

    // 创建 packet
    MppPacket packet = nullptr;
    MPP_RET ret = mpp_packet_init(&packet, impl->simple_packet_buf, size);
    if (ret != MPP_OK) {
        LOG_ERROR("mpp_packet_init 失败");
        return false;
    }
    mpp_packet_set_pts(packet, 0);

    // 发送 packet 到解码器
    ret = impl->mpi->decode_put_packet(impl->ctx, packet);
    mpp_packet_deinit(&packet);

    if (ret != MPP_OK) {
        LOG_WARN("decode_put_packet failed: {}", static_cast<int>(ret));
        // 即使 put_packet 失败，也尝试
        // get_frame，因为可能需要处理帧信息变化或错误
    }

    // 尝试获取解码后的帧
    MppFrame mpp_frame = nullptr;
    MPP_RET get_ret = impl->mpi->decode_get_frame(impl->ctx, &mpp_frame);

    if (get_ret != MPP_OK) {
        if (ret != MPP_OK && get_ret != MPP_OK) {
            // 两个都失败了，那才是真的失败
            return false;
        }
        // 如果 put 成功但 get 失败（通常是没帧），也算本次 decode 过程结束
        // 但如果 put 失败 get 成功（罕见？），或者 put 失败 get 返回 info
        // change...
        LOG_WARN("decode_get_frame failed: {}", static_cast<int>(get_ret));
        return false;
    }

    if (!mpp_frame) {
        // 解码器还在缓冲，需要更多数据
        return false;
    }

    // 检查帧信息变化
    if (mpp_frame_get_info_change(mpp_frame)) {
        out_width = mpp_frame_get_width(mpp_frame);
        out_height = mpp_frame_get_height(mpp_frame);
        out_hor_stride = mpp_frame_get_hor_stride(mpp_frame);
        out_ver_stride = mpp_frame_get_ver_stride(mpp_frame);

        LOG_INFO(
            "帧信息变化: {}x{} stride={}x{}",
            out_width,
            out_height,
            out_hor_stride,
            out_ver_stride
        );

        // 通知解码器信息变化已处理
        ret =
            impl->mpi->control(impl->ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
        if (ret != MPP_OK) {
            LOG_ERROR(
                "MPP_DEC_SET_INFO_CHANGE_READY failed: {}",
                static_cast<int>(ret)
            );
        }

        mpp_frame_deinit(&mpp_frame);
        // DO NOT RETURN FALSE here, try get_frame again?
        // No, typically return false and let calling loop handle it (by sending
        // next packet or retrying?) Actually, if we return false, FilePlayer
        // reads NEW packet. But the previous packet was consumed? Or info
        // change happens internally? Usually need to continue get_frame.
        return false;
    }

    // 检查是否有错误或丢弃帧
    if (mpp_frame_get_errinfo(mpp_frame) || mpp_frame_get_discard(mpp_frame)) {
        LOG_WARN(
            "frame error/discard: err={}, discard={}",
            mpp_frame_get_errinfo(mpp_frame),
            mpp_frame_get_discard(mpp_frame)
        );
        mpp_frame_deinit(&mpp_frame);
        return false;
    }

    // 获取帧缓冲区
    MppBuffer buffer = mpp_frame_get_buffer(mpp_frame);
    if (!buffer) {
        LOG_WARN("frame has no buffer");
        mpp_frame_deinit(&mpp_frame);
        return false;
    }

    // 填充输出结构
    frame.data = mpp_buffer_get_ptr(buffer);
    frame.fd = mpp_buffer_get_fd(buffer);
    frame.width = mpp_frame_get_width(mpp_frame);
    frame.height = mpp_frame_get_height(mpp_frame);
    frame.hor_stride = mpp_frame_get_hor_stride(mpp_frame);
    frame.ver_stride = mpp_frame_get_ver_stride(mpp_frame);

    LOG_DEBUG(
        "Decoded frame: {}x{} stride={}x{} fd={}",
        frame.width,
        frame.height,
        frame.hor_stride,
        frame.ver_stride,
        frame.fd
    );

    // 保存帧引用，下次调用时释放
    if (impl->simple_frame) {
        mpp_frame_deinit(&impl->simple_frame);
    }
    impl->simple_frame = mpp_frame;

    return true;
}

bool MppDecoder::decode(const uint8_t *data, size_t size, DecodedFrame &frame)
{
    if (!m_initialized || !m_impl) {
        return false;
    }

    auto *impl = static_cast<MppDecoderImpl *>(m_impl);

    if (impl->use_simple_api) {
        return decode_simple_mode(
            impl,
            data,
            size,
            frame,
            m_width,
            m_height,
            m_hor_stride,
            m_ver_stride
        );
    } else {
        return decode_task_mode(impl, data, size, frame);
    }
}

} // namespace player
