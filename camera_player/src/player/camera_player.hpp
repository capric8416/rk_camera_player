#pragma once

/**
 * CameraPlayer - 相机播放器
 * 
 * 管理 CameraCapture, MppDecoder, DrmDisplay, MppEncoder
 * 实现 IPlayer 接口
 */

#include "i_player.hpp"
#include "camera_capture.hpp"
#include "mpp_decode.hpp"
#include "mpp_encode.hpp"
#include "drm_display.hpp"

#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <chrono>

namespace player {

class CameraPlayer : public IPlayer {
public:
    CameraPlayer();
    ~CameraPlayer() override;

    // 禁止拷贝
    CameraPlayer(const CameraPlayer &) = delete;
    CameraPlayer &operator=(const CameraPlayer &) = delete;

    // IPlayer 接口实现
    bool init(const PlayerConfig &config) override;
    void run() override;
    void stop() override;
    std::string capture_image() override;
    bool start_video_capture() override;
    std::string stop_video_capture() override;
    bool is_video_capturing() const override { return m_video_capturing; }
    bool is_device_lost() const override { return m_device_lost; }

private:
    /**
     * 查找 JPEG 结束标记
     */
    static size_t find_jpeg_size(
        const uint8_t *data,
        size_t length
    );
    
    /**
     * 保存捕获的图片到文件
     */
    void save_captured_image(
        const uint8_t *jpeg_data,
        size_t jpeg_size
    );
    
    /**
     * 编码一帧视频
     */
    void encode_video_frame(const DecodedFrame &frame);

    CameraCapture m_camera;
    MppDecoder m_decoder;
    MppEncoder m_encoder;
    DrmDisplay m_display;
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_device_lost;
    std::atomic<bool> m_capture_image_requested;
    std::atomic<bool> m_video_capturing;
    std::atomic<bool> m_start_video_requested;
    std::atomic<bool> m_stop_video_requested;
    
    uint32_t m_frame_count;
    PlayerConfig m_config;
    std::string m_images_dir_path;
    std::string m_videos_dir_path;
    std::string m_captured_file_path;
    std::string m_current_video_path;
    std::string m_current_video_basename;
    std::vector<uint8_t> m_first_frame_data;
    uint32_t m_video_frame_count;
    std::chrono::steady_clock::time_point m_video_start_time;
};

} // namespace player
