#ifndef PLAYER_THREAD_H
#define PLAYER_THREAD_H

#include <QThread>
#include <memory>
#include <atomic>

#include "player/i_player.hpp"

namespace player {
class FilePlayer;
}

/**
 * PlayerThread - 在单独线程中运行 Player
 * 
 * 根据 config.camera_device 自动选择 CameraPlayer 或 FilePlayer:
 * - /dev/ 路径使用 CameraPlayer
 * - 其他路径使用 FilePlayer
 */
class PlayerThread : public QThread
{
    Q_OBJECT
public:
    explicit PlayerThread(QObject *parent = nullptr);
    ~PlayerThread();

    void start_player(const player::PlayerConfig &config);
    void stop_player();
    bool is_capturing() const { return m_capturing; }
    
    /**
     * 请求捕获一张图片 (仅 CameraPlayer)
     * @return 保存的文件路径，失败返回空字符串
     */
    QString capture_image();
    
    /**
     * 开始录像 (仅 CameraPlayer)
     * @return 成功返回 true
     */
    bool start_video_capture();
    
    /**
     * 停止录像 (仅 CameraPlayer)
     * @return 录像文件路径
     */
    QString stop_video_capture();
    
    /**
     * 是否正在录像
     */
    bool is_video_capturing() const;

    // FilePlayer 特有方法
    void pause_playback();
    void resume_playback();
    bool is_playback_paused() const;
    void set_start_paused(bool paused);
    void set_meta_info(double fps, uint32_t total_frames);
    uint32_t get_frame_count() const;
    double get_current_time() const;

signals:
    void camera_connected();
    void camera_disconnected();
    void playback_started();
    void playback_ended();
    void image_captured(const QString &filepath);
    void video_started(const QString &filepath);
    void video_stopped(const QString &filepath);

protected:
    void run() override;

private:
    bool is_file_path() const;
    
    std::unique_ptr<player::IPlayer> m_player;
    player::PlayerConfig m_config;
    std::atomic<bool> m_capturing;
    std::atomic<bool> m_stop_requested;
    bool m_start_paused;
    double m_meta_fps;
    uint32_t m_meta_total_frames;
};

#endif // PLAYER_THREAD_H
