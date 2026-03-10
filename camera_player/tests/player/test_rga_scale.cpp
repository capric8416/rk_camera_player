#include <gtest/gtest.h>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include "player/rga_scale.hpp"

using namespace player;

// 辅助函数：读取整个文件
static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) return buffer;
    return {};
}

class RgaScalerTest : public ::testing::Test {
protected:
    void SetUp() override {
        scaler = std::make_unique<RgaScaler>();
    }

    void run_normal_conversion(const std::string& input_yuv, const std::string& output_rgba) {
        if (access(input_yuv.c_str(), F_OK) != 0) {
            GTEST_SKIP() << "Input file " << input_yuv << " not found.";
        }

        std::vector<uint8_t> yuv_data = read_file(input_yuv);
        ASSERT_FALSE(yuv_data.empty());

        ASSERT_TRUE(scaler->init(1280, 720, 640, 360));

        DecodedFrame frame = {};
        frame.width = 1280;
        frame.height = 720;
        frame.hor_stride = 1280;
        frame.ver_stride = 720;
        frame.data = yuv_data.data();
        frame.fd = -1; // 使用虚地址

        std::vector<uint8_t> rgba_data(640 * 360 * 4);
        EXPECT_TRUE(scaler->convert(frame, rgba_data.data(), rgba_data.size()));

        std::ofstream ofs(output_rgba, std::ios::binary);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(reinterpret_cast<const char*>(rgba_data.data()), rgba_data.size());
        ofs.close();

        struct stat st;
        EXPECT_EQ(stat(output_rgba.c_str(), &st), 0);
        EXPECT_EQ(st.st_size, rgba_data.size());
        
        printf("Converted %s -> %s\n", input_yuv.c_str(), output_rgba.c_str());
    }

    std::unique_ptr<RgaScaler> scaler;
};

TEST_F(RgaScalerTest, InitAndDestroy) {
    EXPECT_TRUE(scaler->init(1280, 720, 640, 360));
    scaler->destroy();
}

TEST_F(RgaScalerTest, InitEdgeCases) {
    // Zero dimensions
    EXPECT_FALSE(scaler->init(0, 720, 640, 360));
    EXPECT_FALSE(scaler->init(1280, 0, 640, 360));
    EXPECT_FALSE(scaler->init(1280, 720, 0, 360));
    EXPECT_FALSE(scaler->init(1280, 720, 640, 0));

    // Extremely large dimensions
    EXPECT_FALSE(scaler->init(10000, 720, 640, 360));
    EXPECT_FALSE(scaler->init(1280, 10000, 640, 360));
}

TEST_F(RgaScalerTest, ConvertEdgeCases) {
    DecodedFrame frame = {};
    frame.width = 1280;
    frame.height = 720;
    frame.hor_stride = 1280;
    frame.ver_stride = 720;
    frame.fd = -1; // Mock FD

    std::vector<uint8_t> dst(640 * 360 * 4);

    // Case 1: Convert before init
    EXPECT_FALSE(scaler->convert(frame, dst.data(), dst.size()));

    // Case 2: Init and then test param validation
    ASSERT_TRUE(scaler->init(1280, 720, 640, 360));

    // Null dst_data
    EXPECT_FALSE(scaler->convert(frame, nullptr, dst.size()));

    // Insufficient buffer size
    EXPECT_FALSE(scaler->convert(frame, dst.data(), 100));
}

TEST_F(RgaScalerTest, ConvertNV12ToRGBA_Hardware_ParamCheck) {
    ASSERT_TRUE(scaler->init(1280, 720, 640, 360));
    
    DecodedFrame frame = {};
    frame.width = 1280;
    frame.height = 720;
    frame.hor_stride = 1280;
    frame.ver_stride = 720;
    frame.fd = -1; // No real buffer, data is null
    frame.data = nullptr;
    
    std::vector<uint8_t> dst(640 * 360 * 4);
    
    // This will likely fail in RGA driver due to no address, but should not crash
    EXPECT_FALSE(scaler->convert(frame, dst.data(), dst.size()));
}

TEST_F(RgaScalerTest, ConvertNormalH264) {
    run_normal_conversion("data/out_h264.yuv", "data/out_h264.rgba");
}

TEST_F(RgaScalerTest, ConvertNormalHEVC) {
    run_normal_conversion("data/out_hevc.yuv", "data/out_hevc.rgba");
}

TEST_F(RgaScalerTest, ConvertNormalMJPEG) {
    run_normal_conversion("data/out_mjpeg.yuv", "data/out_mjpeg.rgba");
}

