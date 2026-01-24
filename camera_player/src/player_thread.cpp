#include "player_thread.hpp"
#include "player/camera_player.hpp"
#include "player/file_player.hpp"
#include "player/logger.hpp"

PlayerThread::PlayerThread(QObject *parent)
    : QThread(parent)
    , m_capturing(false)
    , m_stop_requested(false)
    , m_start_paused(false)
    , m_meta_fps(30.0)
    , m_meta_total_frames(0)
{
}

PlayerThread::~PlayerThread()
{
    stop_player();
}

bool PlayerThread::is_file_path() const
{
    // 如果路径不是以 /dev/ 开头，认为是文件路径
    return m_config.camera_device.find("/dev/") != 0;
}

void PlayerThread::start_player(const player::PlayerConfig &config)
{
    if (isRunning()) {
        return;
    }
    m_config = config;
    m_stop_requested = false;
    start();
}

void PlayerThread::stop_player()
{
    m_stop_requested = true;
    if (m_player) {
        m_player->stop();
    }
    if (isRunning()) {
        wait(3000);  // 等待最多3秒
        if (isRunning()) {
            terminate();
            wait();
        }
    }
}

void PlayerThread::run()
{
    if (is_file_path()) {
        // 创建 FilePlayer
        LOG_INFO("Creating FilePlayer for: {}", m_config.camera_device);
        auto file_player = std::make_unique<player::FilePlayer>();
        
        // 设置 meta 信息
        file_player->set_meta_info(m_meta_fps, m_meta_total_frames);
        file_player->set_start_paused(m_start_paused);
        
        m_player = std::move(file_player);
        
        if (!m_player->init(m_config)) {
            LOG_ERROR("FilePlayer init failed");
            m_capturing = false;
            emit playback_ended();
            return;
        }
        
        m_capturing = true;
        emit playback_started();
        
        // 运行播放器主循环
        m_player->run();
        
        m_capturing = false;
        emit playback_ended();
    } else {
        // 创建 CameraPlayer
        LOG_INFO("Creating CameraPlayer for: {}", m_config.camera_device);
        m_player = std::make_unique<player::CameraPlayer>();
        
        if (!m_player->init(m_config)) {
            LOG_ERROR("CameraPlayer init failed");
            m_capturing = false;
            emit camera_disconnected();
            return;
        }
        
        m_capturing = true;
        emit camera_connected();
        
        // 运行播放器主循环
        m_player->run();
        
        // 检查是否是设备丢失
        if (m_player->is_device_lost()) {
            LOG_WARN("Camera device lost");
        }
        
        m_capturing = false;
        emit camera_disconnected();
    }
    
    m_player.reset();
}

QString PlayerThread::capture_image()
{
    if (!m_player || !m_capturing) {
        LOG_WARN("Cannot capture image: player not running");
        return QString();
    }
    
    std::string filepath = m_player->capture_image();
    QString qFilepath = QString::fromStdString(filepath);
    
    if (!filepath.empty()) {
        emit image_captured(qFilepath);
    }
    
    return qFilepath;
}

bool PlayerThread::start_video_capture()
{
    if (!m_player || !m_capturing) {
        LOG_WARN("Cannot start video: player not running");
        return false;
    }
    
    bool result = m_player->start_video_capture();
    if (result) {
        emit video_started(QString());
    }
    return result;
}

QString PlayerThread::stop_video_capture()
{
    if (!m_player || !m_capturing) {
        LOG_WARN("Cannot stop video: player not running");
        return QString();
    }
    
    std::string filepath = m_player->stop_video_capture();
    QString qFilepath = QString::fromStdString(filepath);
    
    if (!filepath.empty()) {
        emit video_stopped(qFilepath);
    }
    
    return qFilepath;
}

bool PlayerThread::is_video_capturing() const
{
    if (!m_player) {
        return false;
    }
    return m_player->is_video_capturing();
}

// FilePlayer 特有方法
void PlayerThread::set_start_paused(bool paused)
{
    m_start_paused = paused;
}

void PlayerThread::set_meta_info(double fps, uint32_t total_frames)
{
    m_meta_fps = fps;
    m_meta_total_frames = total_frames;
}

void PlayerThread::pause_playback()
{
    if (is_file_path() && m_player) {
        auto* fp = dynamic_cast<player::FilePlayer*>(m_player.get());
        if (fp) {
            fp->pause();
        }
    }
}

void PlayerThread::resume_playback()
{
    if (is_file_path() && m_player) {
        auto* fp = dynamic_cast<player::FilePlayer*>(m_player.get());
        if (fp) {
            fp->resume();
        }
    }
}

bool PlayerThread::is_playback_paused() const
{
    if (is_file_path() && m_player) {
        auto* fp = dynamic_cast<player::FilePlayer*>(m_player.get());
        if (fp) {
            return fp->is_paused();
        }
    }
    return false;
}

uint32_t PlayerThread::get_frame_count() const
{
    if (is_file_path() && m_player) {
        auto* fp = dynamic_cast<player::FilePlayer*>(m_player.get());
        if (fp) {
            return fp->get_frame_count();
        }
    }
    return 0;
}

double PlayerThread::get_current_time() const
{
    if (is_file_path() && m_player) {
        auto* fp = dynamic_cast<player::FilePlayer*>(m_player.get());
        if (fp) {
            return fp->get_current_time();
        }
    }
    return 0.0;
}
