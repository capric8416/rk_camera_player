#include <gtest/gtest.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "player/mpp_encode.hpp"

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

class MppEncoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        encoder = std::make_unique<MppEncoder>();
    }

    void TearDown() override {
        encoder->stop();
        encoder->destroy();
        encoder.reset();
    }

    std::unique_ptr<MppEncoder> encoder;
};

TEST_F(MppEncoderTest, InitAndDestroy) {
    EncoderConfig config;
    config.width = 1280;
    config.height = 720;
    config.codec = EncoderCodec::H264;
    config.output_path = "data/test_init.h264";
    config.use_mp4_container = false;

    EXPECT_TRUE(encoder->init(config));
    
    // 重复初始化应失败
    EXPECT_FALSE(encoder->init(config));
    
    encoder->destroy();
    
    // 销毁后重启初始化应成功
    EXPECT_TRUE(encoder->init(config));
    encoder->destroy();
}

TEST_F(MppEncoderTest, InitEdgeCases) {
    EncoderConfig config;
    config.width = 0;
    config.height = 720;
    config.codec = EncoderCodec::H264;
    config.output_path = "";
    config.use_mp4_container = false;

    // 无效宽度
    EXPECT_FALSE(encoder->init(config));

    // 无效高度
    config.width = 1280;
    config.height = 0;
    EXPECT_FALSE(encoder->init(config));

    // 超大尺寸
    config.width = 100000;
    config.height = 100000;
    EXPECT_FALSE(encoder->init(config));
}

TEST_F(MppEncoderTest, EncodeEdgeCases) {
    // 没 start() 调用 encode
    InputFrame dummy_frame = {};
    uint8_t dummy_data[100] = {0};
    dummy_frame.data = dummy_data;
    dummy_frame.size = sizeof(dummy_data);
    dummy_frame.fd = -1;
    dummy_frame.hor_stride = 1280;
    dummy_frame.ver_stride = 720;

    EXPECT_FALSE(encoder->encode_frame(dummy_frame));

    EncoderConfig config;
    config.width = 1280;
    config.height = 720;
    config.codec = EncoderCodec::H264;
    config.output_path = "";
    config.use_mp4_container = false;

    ASSERT_TRUE(encoder->init(config));
    ASSERT_TRUE(encoder->start());

    // data = nullptr 且 fd = -1
    InputFrame null_frame = dummy_frame;
    null_frame.data = nullptr;
    EXPECT_FALSE(encoder->encode_frame(null_frame));

    // size = 0 且 fd = -1
    InputFrame zero_size_frame = dummy_frame;
    zero_size_frame.size = 0;
    EXPECT_FALSE(encoder->encode_frame(zero_size_frame));

    // size 超大
    InputFrame large_frame = dummy_frame;
    large_frame.size = 1280 * 720 * 3; // impl->frame_size 应该是 1280*720*1.5 左右
    EXPECT_FALSE(encoder->encode_frame(large_frame));

    encoder->stop();
    encoder->destroy();
}

TEST_F(MppEncoderTest, EncodeMJPEG) {
    std::string input_file = "data/out_mjpeg.yuv";
    std::string output_file = "data/out.jpg";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found. Run MppDecoder tests first.";
    }

    EncoderConfig config;
    config.width = 1280;
    config.height = 720;
    config.codec = EncoderCodec::MJPEG;
    config.output_path = output_file;
    config.use_mp4_container = false; // MJPEG 不封装 mp4

    ASSERT_TRUE(encoder->init(config));
    ASSERT_TRUE(encoder->start());

    std::vector<uint8_t> yuv_data = read_file(input_file);
    ASSERT_FALSE(yuv_data.empty());

    InputFrame frame = {};
    frame.data = yuv_data.data();
    frame.size = yuv_data.size();
    frame.fd = -1;
    frame.hor_stride = 1280;
    frame.ver_stride = 720;

    EXPECT_TRUE(encoder->encode_frame(frame));
    encoder->stop();

    // 验证输出文件
    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
    
    printf("\n[Check Result] ffplay %s\n", output_file.c_str());
}

TEST_F(MppEncoderTest, EncodeH264) {
    std::string input_file = "data/out_h264.yuv";
    std::string output_file = "data/out.h264";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found. Run MppDecoder tests first.";
    }

    EncoderConfig config;
    config.width = 1280;
    config.height = 720;
    config.codec = EncoderCodec::H264;
    config.output_path = output_file;
    config.use_mp4_container = false; // 测试原始流

    ASSERT_TRUE(encoder->init(config));
    ASSERT_TRUE(encoder->start());

    std::vector<uint8_t> yuv_data = read_file(input_file);
    ASSERT_FALSE(yuv_data.empty());

    InputFrame frame = {};
    frame.data = yuv_data.data();
    frame.size = yuv_data.size();
    frame.fd = -1;
    frame.hor_stride = 1280;
    frame.ver_stride = 720;

    // 编码 30 帧画面 (约 1 秒)
    for(int i=0; i<30; ++i) {
        EXPECT_TRUE(encoder->encode_frame(frame));
    }
    encoder->stop();

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
    
    printf("\n[Check Result] ffplay -f h264 %s\n", output_file.c_str());
}

TEST_F(MppEncoderTest, EncodeHEVC) {
    std::string input_file = "data/out_hevc.yuv";
    std::string output_file = "data/out.h265";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found. Run MppDecoder tests first.";
    }

    EncoderConfig config;
    config.width = 1280;
    config.height = 720;
    config.codec = EncoderCodec::HEVC;
    config.output_path = output_file;
    config.use_mp4_container = false;

    ASSERT_TRUE(encoder->init(config));
    ASSERT_TRUE(encoder->start());

    std::vector<uint8_t> yuv_data = read_file(input_file);
    ASSERT_FALSE(yuv_data.empty());

    InputFrame frame = {};
    frame.data = yuv_data.data();
    frame.size = yuv_data.size();
    frame.fd = -1;
    frame.hor_stride = 1280;
    frame.ver_stride = 720;

    for(int i=0; i<30; ++i) {
        EXPECT_TRUE(encoder->encode_frame(frame));
    }
    encoder->stop();

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
    
    printf("\n[Check Result] ffplay -f hevc %s\n", output_file.c_str());
}
