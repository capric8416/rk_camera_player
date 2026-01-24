#pragma once

/**
 * FilePlayer - 文件播放器
 * 
 * 播放本地 H.264/H.265 视频文件
 * 实现 IPlayer 接口
 */

#include "i_player.hpp"
#include "mpp_decode.hpp"
#include "drm_display.hpp"
#include "ffmpeg_demuxer.hpp"

#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace player {

class FilePlayer : public IPlayer {
public:
    FilePlayer();
    ~FilePlayer() override;

    // 禁止拷贝
    FilePlayer(const FilePlayer &) = delete;
    FilePlayer &operator=(const FilePlayer &) = delete;

    // IPlayer 接口实现
    bool init(const PlayerConfig &config) override;
    void run() override;
    void stop() override;
    
    // FilePlayer 不支持拍照和录像，返回空/false
    std::string capture_image() override { return ""; }
    bool start_video_capture() override { return false; }
    std::string stop_video_capture() override { return ""; }
    bool is_video_capturing() const override { return false; }
    bool is_device_lost() const override { return m_eof_reached; }

    // 播放控制
    void pause();
    void resume();
    bool is_paused() const { return m_paused; }
    
    // 设置初始状态为暂停（显示首帧后暂停）
    void set_start_paused(bool paused) { m_start_paused = paused; }
    
    // 设置从 meta 文件解析的 fps 和总帧数 (对于 MP4，优先使用文件内信息)
    void set_meta_info(double fps, uint32_t total_frames);

    /**
     * 获取已解码帧数
     */
    uint32_t get_frame_count() const { return m_frame_count; }

    /**
     * 获取 FPS (优先使用文件内信息)
     */
    double get_fps() const { return m_fps > 0 ? m_fps : m_meta_fps; }

    /**
     * 获取当前播放时间（秒）
     */
    double get_current_time() const;

    /**
     * 获取当前播放文件名
     */
    std::string get_current_filename() const;

private:
    /**
     * 显示一帧 (读取+解码+显示)
     * @return 成功解码并显示返回 true
     */
    bool decode_and_display_frame();

    std::unique_ptr<FFmpegDemuxer> m_demuxer;
    std::string m_filepath;

    MppDecoder m_decoder;
    DrmDisplay m_display;

    PlayerConfig m_config;
    std::atomic<bool> m_running;
    std::atomic<bool> m_eof_reached;
    std::atomic<bool> m_paused;
    bool m_start_paused;
    
    // 暂停/恢复同步
    std::mutex m_pause_mutex;
    std::condition_variable m_pause_cv;
    
    uint32_t m_frame_count;
    double m_fps;
    double m_meta_fps;          // 来自 meta 的 fps
    uint32_t m_meta_total_frames; // 来自 meta 的总帧数
};

} // namespace player
