#include "file_player.hpp"
#include "logger.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

namespace player {

FilePlayer::FilePlayer()
    : m_running(false)
    , m_eof_reached(false)
    , m_paused(false)
    , m_start_paused(false)
    , m_frame_count(0)
    , m_fps(0.0)
    , m_meta_fps(30.0)
    , m_meta_total_frames(0)
{
}

FilePlayer::~FilePlayer() { stop(); }

bool FilePlayer::init(const PlayerConfig &config)
{
    m_config = config;
    m_filepath = config.camera_device; // 复用 camera_device 字段

    // 创建并打开 Demuxer
    m_demuxer = std::make_unique<FFmpegDemuxer>();
    if (!m_demuxer->open(m_filepath)) {
        LOG_ERROR("无法打开视频文件: {}", m_filepath);
        return false;
    }

    // 优先使用 Demuxer 获取的 FPS
    m_fps = m_demuxer->get_fps();
    if (m_fps <= 0) {
        LOG_WARN("Demuxer 未获取到有效 FPS，将使用默认值或 meta 信息");
    }

    // 初始化解码器
    MppCodingType coding_type =
        m_demuxer->is_hevc() ? MPP_VIDEO_CodingHEVC : MPP_VIDEO_CodingAVC;

    // 使用 Demuxer 获取的分辨率来初始化解码器
    // 注意：如果分辨率变化，解码器会在运行时自动调整，这里只是初始推荐值
    uint32_t width = m_demuxer->get_width();
    uint32_t height = m_demuxer->get_height();
    if (width == 0 || height == 0) {
        width = config.width;
        height = config.height;
    }

    if (!m_decoder.init(width, height, coding_type)) {
        LOG_ERROR("解码器初始化失败");
        return false;
    }

    // 初始化 DRM 显示
    if (!m_display.open(config.drm_device)) {
        LOG_ERROR("DRM 打开失败");
        return false;
    }

    if (!m_display
             .init(config.width, config.height, config.zpos, config.plane_id)) {
        LOG_ERROR("DRM 初始化失败");
        return false;
    }

    return true;
}

void FilePlayer::set_meta_info(double fps, uint32_t total_frames)
{
    m_meta_fps = fps > 0 ? fps : 30.0;
    m_meta_total_frames = total_frames;
    LOG_INFO(
        "设置 meta 信息: fps={:.2f}, total_frames={}",
        m_meta_fps,
        m_meta_total_frames
    );
}

void FilePlayer::pause()
{
    m_paused = true;
    LOG_INFO("播放暂停");
}

void FilePlayer::resume()
{
    {
        std::lock_guard<std::mutex> lock(m_pause_mutex);
        m_paused = false;
    }
    m_pause_cv.notify_one();
    LOG_INFO("播放恢复");
}

double FilePlayer::get_current_time() const
{
    double fps = get_fps();
    if (fps <= 0)
        return 0.0;
    return static_cast<double>(m_frame_count) / fps;
}

std::string FilePlayer::get_current_filename() const
{
    return std::filesystem::path(m_filepath).filename().string();
}

bool FilePlayer::decode_and_display_frame()
{
    std::vector<uint8_t> packet_data;
    int64_t pts = 0;

    // 循环读取直到解码成功显示一帧
    while (m_running && !m_eof_reached) {
        // 从 Demuxer 读取包 (Annex-B 格式)
        if (!m_demuxer->read_packet(packet_data, pts)) {
            LOG_INFO("文件播放结束 (EOF)");
            m_eof_reached = true;
            return false;
        }

        if (packet_data.empty()) {
            continue;
        }

        // 解码
        DecodedFrame decoded;
        if (!m_decoder
                 .decode(packet_data.data(), packet_data.size(), decoded)) {
            // 解码器可能需要更多数据才能输出一帧，或者这是一个不产生显示的包
            continue;
        }

        // MPP 解码器有时返回宽度不一致的帧（裁剪尺寸 vs 步长对齐尺寸），
        // DrmDisplay 处理时会使用 decoded.width/height 进行显示区域设置，如果是
        // NV12， RGA 或者 DRM Plane 设置可以处理。 这里直接使用 DrmDisplay 显示
        if (!m_display.display_nv12(
                decoded.fd,
                decoded.width,
                decoded.height,
                decoded.hor_stride,
                decoded.ver_stride
            )) {
            LOG_ERROR("NV12 显示失败");
            continue;
        }

        ++m_frame_count;
        return true;
    }

    return false;
}

void FilePlayer::run()
{
    m_running = true;
    m_eof_reached = false;
    m_frame_count = 0;

    LOG_INFO("开始播放: {}, start_paused={}", m_filepath, m_start_paused);

    double current_fps = get_fps();
    if (current_fps <= 0)
        current_fps = 30.0;

    auto frame_duration =
        std::chrono::microseconds(static_cast<int>(1000000.0 / current_fps));

    // 如果需要初始暂停，先解码显示首帧
    if (m_start_paused) {
        if (decode_and_display_frame()) {
            LOG_INFO("显示首帧后暂停");
            m_paused = true;
        }
    }

    auto next_frame_time = std::chrono::steady_clock::now();

    while (m_running) {
        // 检查暂停状态
        if (m_paused) {
            std::unique_lock<std::mutex> lock(m_pause_mutex);
            m_pause_cv.wait(lock, [this] { return !m_paused || !m_running; });
            if (!m_running)
                break;
            next_frame_time = std::chrono::steady_clock::now();
        }

        // 解码并显示一帧
        if (!decode_and_display_frame()) {
            // EOF 处理：循环播放
            if (m_eof_reached && m_running) {
                LOG_INFO("播放循环: 重置到文件开头");

                // Seek 到 0
                if (m_demuxer->seek(0)) {
                    m_eof_reached = false;
                    m_frame_count = 0;

                    // 重新初始化解码器以清除状态
                    m_decoder.destroy();
                    MppCodingType coding_type = m_demuxer->is_hevc()
                                                    ? MPP_VIDEO_CodingHEVC
                                                    : MPP_VIDEO_CodingAVC;
                    m_decoder.init(
                        m_demuxer->get_width(),
                        m_demuxer->get_height(),
                        coding_type
                    );

                    // 解码首帧并暂停（类似之前逻辑）
                    while (m_running && !m_eof_reached) {
                        if (decode_and_display_frame()) {
                            LOG_INFO("循环开始：显示首帧后暂停");
                            m_paused = true;
                            break;
                        }
                    }
                    if (m_paused)
                        continue;
                } else {
                    LOG_ERROR("Seek 失败，停止播放");
                    break;
                }
            } else {
                break;
            }
        }

        // 帧率控制
        next_frame_time += frame_duration;
        auto now = std::chrono::steady_clock::now();
        if (next_frame_time > now) {
            std::this_thread::sleep_until(next_frame_time);
        }

        // 统计
        if (get_fps() > 0 &&
            m_frame_count % static_cast<uint32_t>(get_fps()) == 0) {
            LOG_INFO(
                "播放帧数: {}, 时间: {:.1f}s",
                m_frame_count,
                get_current_time()
            );
        }
    }

    LOG_INFO("播放结束, 共 {} 帧", m_frame_count);
}

void FilePlayer::stop()
{
    m_running = false;
    {
        std::lock_guard<std::mutex> lock(m_pause_mutex);
        m_paused = false;
    }
    m_pause_cv.notify_one();

    // Demuxer 会在析构时关闭，或在 init 时重新打开
}

} // namespace player
