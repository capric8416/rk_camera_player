#include <gtest/gtest.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>

#include "player/mpp_decode.hpp"

using namespace player;

// 辅助函数：读取整个文件
static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

// 辅助函数：将解码出的 NV12 数据保存到文件
static void save_yuv_file(const std::string& path, const DecodedFrame& frame) {
    if (!frame.data) return;
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    // NV12 的 Y 亮度和 UV 色度分量大小
    size_t y_size = frame.hor_stride * frame.ver_stride;
    size_t uv_size = frame.hor_stride * frame.ver_stride / 2;

    file.write(reinterpret_cast<const char*>(frame.data), y_size + uv_size);
}

class MppDecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        decoder = std::make_unique<MppDecoder>();
    }

    void TearDown() override {
        decoder.reset();
    }

    std::unique_ptr<MppDecoder> decoder;
};

// ============================================================================
// 1. 初始化与销毁测试
// ============================================================================

TEST_F(MppDecoderTest, InitAndDestroy) {
    // 正常初始化 MJPEG
    EXPECT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG));
    decoder->destroy();

    // 正常初始化 H264
    EXPECT_TRUE(decoder->init(1920, 1080, MPP_VIDEO_CodingAVC));
    decoder->destroy();

    // 正常初始化 HEVC (H265)
    EXPECT_TRUE(decoder->init(1920, 1080, MPP_VIDEO_CodingHEVC));
    decoder->destroy();
    
    // 多次 destroy 安全性
    decoder->destroy();
    EXPECT_NO_FATAL_FAILURE(decoder->destroy());

    // 异常尺寸初始化
    EXPECT_FALSE(decoder->init(0, 0, MPP_VIDEO_CodingAVC));
    EXPECT_FALSE(decoder->init(100000, 100000, MPP_VIDEO_CodingAVC));

    // 重复初始化冲突测试
    EXPECT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG));
    EXPECT_FALSE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG)); 
    decoder->destroy();
    EXPECT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG));
    decoder->destroy();

    // 不支持的编码类型
    EXPECT_FALSE(decoder->init(1280, 720, (MppCodingType)999));
}

TEST_F(MppDecoderTest, DecodeEdgeCases) {
    DecodedFrame frame = {};
    uint8_t dummy_data[100] = {0};

    // 未初始化去 decode
    EXPECT_FALSE(decoder->decode(dummy_data, sizeof(dummy_data), frame));

    // 初始化后异常参数测试
    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG));
    
    // nullptr 和 0 size
    EXPECT_FALSE(decoder->decode(nullptr, 0, frame));
    EXPECT_FALSE(decoder->decode(nullptr, 10, frame));

    // 超大 packet size 测试 (H264)
    decoder->destroy();
    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingAVC));
    EXPECT_FALSE(decoder->decode(dummy_data, 1280 * 720 * 3, frame)); // simple_packet_buf_size 是 width*height*2

    // 随机垃圾数据码流容错测试
    decoder->destroy();
    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingAVC));
    uint8_t garbage[1024];
    for(int i=0; i<1024; ++i) garbage[i] = rand() % 256;
    
    // 垃圾数据可能被 decoder 接收但无法出帧，或者直接 put_packet 失败，但不应 crash
    bool res = decoder->decode(garbage, sizeof(garbage), frame);
    EXPECT_FALSE(res);
}

// ============================================================================
// 2. 解码功能测试 (需要真实环境与测试数据)
// ============================================================================

TEST_F(MppDecoderTest, DecodeMJPEG) {
    std::string test_file = "data/test.jpg";
    if (access(test_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Test file " << test_file << " not found, skipping.";
    }

    std::vector<uint8_t> data = read_file(test_file);
    ASSERT_FALSE(data.empty()) << "Failed to read " << test_file;

    // 假设测试图片是 1280x720 (实际使用 MPP 解码时最好提供真实宽高)
    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingMJPEG));

    DecodedFrame frame = {};
    bool decoded = decoder->decode(data.data(), data.size(), frame);
    
    EXPECT_TRUE(decoded);
    if (decoded) {
        EXPECT_GT(frame.width, 0);
        EXPECT_GT(frame.height, 0);
        EXPECT_GE(frame.hor_stride, frame.width);
        EXPECT_GE(frame.ver_stride, frame.height);
        EXPECT_GT(frame.fd, 0);
        EXPECT_NE(frame.data, nullptr);
        
        save_yuv_file("data/out_mjpeg.yuv", frame);
    }
}

TEST_F(MppDecoderTest, DecodeH264) {
    std::string test_file = "data/test.h264";
    if (access(test_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Test file " << test_file << " not found, skipping.";
    }

    std::vector<uint8_t> data = read_file(test_file);
    ASSERT_FALSE(data.empty()) << "Failed to read " << test_file;

    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingAVC));

    DecodedFrame frame = {};
    bool frame_produced = false;

    // 第一次推入完整数据（一帧）
    if (decoder->decode(data.data(), data.size(), frame)) {
        frame_produced = true;
    } else {
        // MPP 可能会返回 INFO_CHANGE，接着我们轮询获取真正的帧数据
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 给解码器一点时间运行
            if (decoder->decode(nullptr, 0, frame)) {
                frame_produced = true;
                break;
            }
        }
    }

    EXPECT_TRUE(frame_produced);
    if (frame_produced) {
        EXPECT_GT(frame.width, 0);
        EXPECT_GT(frame.height, 0);
        EXPECT_GT(frame.fd, 0);
        EXPECT_NE(frame.data, nullptr);
        
        save_yuv_file("data/out_h264.yuv", frame);
    }
}

TEST_F(MppDecoderTest, DecodeHEVC) {
    std::string test_file = "data/test.h265";
    if (access(test_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Test file " << test_file << " not found, skipping.";
    }

    std::vector<uint8_t> data = read_file(test_file);
    ASSERT_FALSE(data.empty()) << "Failed to read " << test_file;

    ASSERT_TRUE(decoder->init(1280, 720, MPP_VIDEO_CodingHEVC));

    DecodedFrame frame = {};
    bool frame_produced = false;

    // 第一次推入完整数据（一帧）
    if (decoder->decode(data.data(), data.size(), frame)) {
        frame_produced = true;
    } else {
        // MPP 可能会返回 INFO_CHANGE，接着我们轮询获取真正的帧数据
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 给解码器一点时间运行
            if (decoder->decode(nullptr, 0, frame)) {
                frame_produced = true;
                break;
            }
        }
    }

    EXPECT_TRUE(frame_produced);
    if (frame_produced) {
        EXPECT_GT(frame.width, 0);
        EXPECT_GT(frame.height, 0);
        EXPECT_GT(frame.fd, 0);
        EXPECT_NE(frame.data, nullptr);
        
        save_yuv_file("data/out_hevc.yuv", frame);
    }
}

/*
ffplay -f rawvideo -pixel_format nv12 -video_size 1280x720 data/out_mjpeg.yuv
# 预期 test.jpg 是橙色
ffplay -f rawvideo -pixel_format nv12 -video_size 1280x720 data/out_h264.yuv
# 预期 test.h264 是蓝色
ffplay -f rawvideo -pixel_format nv12 -video_size 1280x720 data/out_hevc.yuv
# 预期 test.h265 是绿色
*/
