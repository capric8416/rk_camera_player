#include "camera_player.hpp"
#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <thread>
#include <filesystem>

#define IMAGE_FILENAME_FORMAT "{:04d}-{:02d}-{:02d}_{:02d}-{:02d}-{:02d}_{:03d}.jpeg"
#define IMAGE_FILENAME_EXT ".jpeg"
#define VIDEO_FILENAME_FORMAT "{:04d}-{:02d}-{:02d}_{:02d}-{:02d}-{:02d}_{:03d}"
#define VIDEO_FILENAME_EXT ".mp4"

namespace player {

CameraPlayer::CameraPlayer()
    : m_running(false)
    , m_device_lost(false)
    , m_capture_image_requested(false)
    , m_video_capturing(false)
    , m_start_video_requested(false)
    , m_stop_video_requested(false)
    , m_frame_count(0)
{
}

CameraPlayer::~CameraPlayer() {
    stop();
}

bool CameraPlayer::init(const PlayerConfig &config) {
    m_config = config;
    m_images_dir_path = config.images_dir_path;
    m_videos_dir_path = config.videos_dir_path;
    
    // 创建保存目录
    std::filesystem::create_directories(m_images_dir_path);
    std::filesystem::create_directories(m_videos_dir_path);
    
    // 打开摄像头
    if (!m_camera.open(config.camera_device, config.width, config.height, config.fps)) {
        LOG_ERROR("摄像头初始化失败");
        return false;
    }

    // 初始化 MPP 解码器
    if (!m_decoder.init(m_camera.width(), m_camera.height())) {
        LOG_ERROR("MPP 解码器初始化失败");
        return false;
    }

    // 打开 DRM 显示
    if (!m_display.open(config.drm_device)) {
        LOG_ERROR("DRM 打开失败");
        return false;
    }

    if (!m_display.init(m_camera.width(), m_camera.height(), config.zpos, config.plane_id)) {
        LOG_ERROR("DRM 初始化失败");
        return false;
    }

    // 开始摄像头采集
    if (!m_camera.start_streaming()) {
        LOG_ERROR("启动采集失败");
        return false;
    }

    return true;
}

void CameraPlayer::run() {
    m_running = true;
    m_frame_count = 0;

    auto start_time = std::chrono::steady_clock::now();

    LOG_INFO("开始运行 (Ctrl+C 退出)...");

    while (m_running) {
        // 1. 采集帧
        uint8_t *jpeg_data = nullptr;
        size_t jpeg_size = 0;

        if (!m_camera.capture_frame(&jpeg_data, &jpeg_size)) {
            // 设备可能断开，退出循环
            LOG_WARN("采集帧失败，设备可能断开");
            m_running = false;
            m_device_lost = true;
            break;
        }

        // 计算实际 JPEG 大小
        size_t actual_size = find_jpeg_size(jpeg_data, jpeg_size);

        // 检查是否有捕获图片请求
        if (m_capture_image_requested.exchange(false)) {
            save_captured_image(jpeg_data, actual_size);
        }

        // 2. MPP 解码
        DecodedFrame decoded;
        if (!m_decoder.decode(jpeg_data, actual_size, decoded)) {
            LOG_ERROR("MPP 解码失败");
            m_camera.release_frame();
            continue;
        }

        // 检查是否需要开始录像
        if (m_start_video_requested.exchange(false)) {
            // 生成视频文件名
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            std::tm *tm_now = std::localtime(&time_t_now);
            
            // 保存 base name 用于元数据匹配
            m_current_video_basename = fmt::format(
                VIDEO_FILENAME_FORMAT,
                tm_now->tm_year + 1900,
                tm_now->tm_mon + 1,
                tm_now->tm_mday,
                tm_now->tm_hour,
                tm_now->tm_min,
                tm_now->tm_sec,
                static_cast<int>(ms.count())
            );
            
            std::string filename = m_current_video_basename + VIDEO_FILENAME_EXT;
            m_current_video_path = m_videos_dir_path + "/" + filename;
            
            // 记录首帧数据和开始时间
            if (jpeg_data && actual_size > 0) {
                m_first_frame_data.assign(jpeg_data, jpeg_data + actual_size);
            }
            m_video_start_time = std::chrono::steady_clock::now();
            m_video_frame_count = 0;
            
            // 配置并初始化编码器
            EncoderConfig enc_cfg;
            enc_cfg.width = m_config.width;
            enc_cfg.height = m_config.height;
            enc_cfg.fps = m_config.fps;
            enc_cfg.gop = m_config.video_gop;
            enc_cfg.bitrate = m_config.video_bitrate;
            enc_cfg.codec = m_config.video_codec;
            enc_cfg.rc_mode = m_config.video_rc_mode;
            enc_cfg.pix_fmt = EncoderPixelFormat::NV12;
            enc_cfg.output_path = m_current_video_path;
            
            // MP4 容器配置: fragmented + empty_moov
            enc_cfg.use_mp4_container = true;
            enc_cfg.mp4_fragmented = true;
            enc_cfg.mp4_faststart = false;
            
            if (m_encoder.init(enc_cfg) && m_encoder.start()) {
                m_video_capturing = true;
                LOG_INFO("开始录像: {}", m_current_video_path);
            } else {
                LOG_ERROR("开始录像失败");
                m_current_video_path.clear();
            }
        }

        // 检查是否需要停止录像
        if (m_stop_video_requested.exchange(false)) {
            if (m_video_capturing) {
                // 计算统计信息
                auto end_time = std::chrono::steady_clock::now();
                double duration = std::chrono::duration<double>(end_time - m_video_start_time).count();
                double fps = (duration > 0) ? (m_video_frame_count / duration) : 0;
                
                m_encoder.stop();
                m_encoder.destroy();
                m_video_capturing = false;
                LOG_INFO("停止录像: {}, frames={}, duration={:.2f}s, fps={:.2f}", 
                    m_current_video_path, m_video_frame_count, duration, fps);
                    
                // 保存元数据 (首帧图片)
                // 格式: YYYY-MM-DD_HH-MM-SS-sss_fps_frames.jpeg
                // 注意: fps 可能是浮点数，保留2位小数
                if (!m_first_frame_data.empty()) {
                    std::string meta_dir = m_config.videos_meta_dir_path;
                    std::filesystem::create_directories(meta_dir);
                    
                    std::string meta_filename = fmt::format(
                        "{}_{:.2f}_{}{}",
                        m_current_video_basename, fps, m_video_frame_count, IMAGE_FILENAME_EXT
                    );
                    
                    std::string meta_path = meta_dir + "/" + meta_filename;
                    std::ofstream meta_file(meta_path, std::ios::binary);
                    if (meta_file.is_open()) {
                        meta_file.write(reinterpret_cast<const char*>(m_first_frame_data.data()), m_first_frame_data.size());
                        meta_file.close();
                        LOG_INFO("保存元数据: {}", meta_path);
                    } else {
                        LOG_ERROR("保存元数据失败: {}", meta_path);
                    }
                    m_first_frame_data.clear();
                }
            }
        }

        // 如果正在录像，编码当前帧
        if (m_video_capturing) {
            encode_video_frame(decoded);
            m_video_frame_count++;
        }

        // 3. 直接显示 NV12 (跳过 RGA 转换)
        if (!m_display.display_nv12(decoded.fd, decoded.width, decoded.height,
                                    decoded.hor_stride, decoded.ver_stride)) {
            LOG_ERROR("NV12 显示失败");
            m_camera.release_frame();
            continue;
        }

        // 5. 释放帧
        m_camera.release_frame();

        // 统计
        ++m_frame_count;
        if (m_frame_count % 30 == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            double fps = m_frame_count / elapsed;
            LOG_INFO("帧数: {}, FPS: {:.1f}{}", m_frame_count, fps, 
                m_video_capturing ? " [录像中]" : "");
        }
    }

    // 如果正在录像，停止录像
    if (m_video_capturing) {
        m_encoder.stop();
        m_encoder.destroy();
        m_video_capturing = false;
    }

    LOG_INFO("正在退出... (共显示 {} 帧)", m_frame_count);

    m_camera.stop_streaming();
}

void CameraPlayer::stop() {
    m_running = false;
}

bool CameraPlayer::start_video_capture() {
    if (m_video_capturing) {
        LOG_WARN("已经在录像中");
        return false;
    }
    
    if (m_capture_image_requested) {
        LOG_WARN("正在拍照，无法开始录像");
        return false;
    }
    
    m_start_video_requested = true;
    
    // 等待录像开始 (最多等待1秒)
    for (int i = 0; i < 100 && m_start_video_requested; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return m_video_capturing;
}

std::string CameraPlayer::stop_video_capture() {
    if (!m_video_capturing) {
        LOG_WARN("没有在录像");
        return "";
    }
    
    m_stop_video_requested = true;
    
    // 等待录像停止 (最多等待1秒)
    for (int i = 0; i < 100 && m_video_capturing; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return m_current_video_path;
}

void CameraPlayer::encode_video_frame(const DecodedFrame &frame) {
    InputFrame input;
    input.data = frame.data;
    input.fd = frame.fd;
    input.size = frame.hor_stride * frame.ver_stride * 3 / 2;  // NV12 size
    input.hor_stride = frame.hor_stride;
    input.ver_stride = frame.ver_stride;
    
    if (!m_encoder.encode_frame(input)) {
        LOG_WARN("编码帧失败");
    }
}

void CameraPlayer::save_captured_image(const uint8_t *jpeg_data, size_t jpeg_size) {
    // 生成文件名: yyyy-mm-dd_hh-mm-ss_mmm.jpeg
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm *tm_now = std::localtime(&time_t_now);
    
    std::string filename = fmt::format(
        IMAGE_FILENAME_FORMAT,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec,
        static_cast<int>(ms.count())
    );
    
    std::string filepath = m_images_dir_path + "/" + filename;
    
    // 保存 JPEG 文件
    std::ofstream file(filepath, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(jpeg_data), jpeg_size);
        file.close();
        m_captured_file_path = filepath;
        LOG_INFO("图片已保存: {}", filepath);
    } else {
        LOG_ERROR("无法保存图片: {}", filepath);
        m_captured_file_path.clear();
    }
}

std::string CameraPlayer::capture_image() {
    if (m_video_capturing) {
        LOG_WARN("正在录像，无法拍照");
        return "";
    }
    
    m_captured_file_path.clear();
    m_capture_image_requested = true;
    
    // 等待捕获完成 (最多等待500ms)
    for (int i = 0; i < 50 && m_capture_image_requested; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return m_captured_file_path;
}

size_t CameraPlayer::find_jpeg_size(
    const uint8_t *data,
    size_t length
) {
    for (size_t i = 0; i + 1 < length; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) {
            return i + 2;
        }
    }
    return length;
}

} // namespace player
