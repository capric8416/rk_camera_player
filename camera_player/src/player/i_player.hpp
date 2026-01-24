#pragma once

/**
 * IPlayer - 播放器接口基类
 * 
 * 定义播放器的通用接口，供 CameraPlayer 和 VideoPlayer 实现
 */

#include <string>
#include <cstdint>
#include "mpp_encode.hpp"

namespace player {

/**
 * 播放器配置
 */
struct PlayerConfig {
    std::string camera_device = "/dev/video51";
    std::string drm_device = "/dev/dri/card0";
    std::string images_dir_path = "images";
    std::string videos_dir_path = "videos";
    std::string videos_meta_dir_path = "videos/.meta";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t fps = 30;
    int zpos = 0;
    uint32_t plane_id = 0;
    
    // 视频编码参数
    uint32_t video_bitrate = 4000000;
    uint32_t video_gop = 30;
    EncoderCodec video_codec = EncoderCodec::HEVC;
    RateControlMode video_rc_mode = RateControlMode::CBR;
};

/**
 * 播放器接口
 */
class IPlayer {
public:
    virtual ~IPlayer() = default;

    /**
     * 初始化播放器
     */
    virtual bool init(const PlayerConfig &config) = 0;

    /**
     * 运行主循环 (阻塞)
     */
    virtual void run() = 0;

    /**
     * 停止运行
     */
    virtual void stop() = 0;

    /**
     * 捕获并保存一张图片
     * @return 保存的文件路径，失败返回空字符串
     */
    virtual std::string capture_image() = 0;

    /**
     * 开始录像
     * @return 成功返回 true
     */
    virtual bool start_video_capture() = 0;

    /**
     * 停止录像
     * @return 录像文件路径
     */
    virtual std::string stop_video_capture() = 0;

    /**
     * 是否正在录像
     */
    virtual bool is_video_capturing() const = 0;

    /**
     * 检查设备是否丢失
     */
    virtual bool is_device_lost() const = 0;
};

} // namespace player
