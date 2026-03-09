#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "player/ffmpeg_muxer.hpp"

using namespace player;

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) return buffer;
    return {};
}

static std::vector<std::vector<uint8_t>> extract_nal_units(const std::vector<uint8_t>& stream_data) {
    std::vector<std::vector<uint8_t>> nalus;
    if (stream_data.empty()) return nalus;

    size_t i = 0;
    size_t nalu_start = 0;
    bool found_first = false;

    while (i + 2 < stream_data.size()) {
        bool is_start_code = false;
        size_t start_code_len = 0;

        if (stream_data[i] == 0x00 && stream_data[i+1] == 0x00 && stream_data[i+2] == 0x01) {
            is_start_code = true;
            start_code_len = 3;
        } else if (i + 3 < stream_data.size() && stream_data[i] == 0x00 && stream_data[i+1] == 0x00 && stream_data[i+2] == 0x00 && stream_data[i+3] == 0x01) {
            is_start_code = true;
            start_code_len = 4;
        }

        if (is_start_code) {
            if (found_first) {
                size_t nalu_len = i - nalu_start;
                if (nalu_len > 0) {
                    std::vector<uint8_t> nalu(stream_data.begin() + nalu_start, stream_data.begin() + i);
                    nalus.push_back(std::move(nalu));
                }
            }
            nalu_start = i;
            found_first = true;
            i += start_code_len;
        } else {
            i++;
        }
    }

    if (found_first && nalu_start < stream_data.size()) {
        std::vector<uint8_t> nalu(stream_data.begin() + nalu_start, stream_data.end());
        nalus.push_back(std::move(nalu));
    }

    return nalus;
}

static uint8_t get_h264_nal_type(const std::vector<uint8_t>& nalu) {
    size_t start_code_len = 0;
    if (nalu.size() >= 3 && nalu[0] == 0 && nalu[1] == 0 && nalu[2] == 1) start_code_len = 3;
    else if (nalu.size() >= 4 && nalu[0] == 0 && nalu[1] == 0 && nalu[2] == 0 && nalu[3] == 1) start_code_len = 4;
    else return 0;
    if (nalu.size() <= start_code_len) return 0;
    return nalu[start_code_len] & 0x1F;
}

static uint8_t get_h265_nal_type(const std::vector<uint8_t>& nalu) {
    size_t start_code_len = 0;
    if (nalu.size() >= 3 && nalu[0] == 0 && nalu[1] == 0 && nalu[2] == 1) start_code_len = 3;
    else if (nalu.size() >= 4 && nalu[0] == 0 && nalu[1] == 0 && nalu[2] == 0 && nalu[3] == 1) start_code_len = 4;
    else return 255; // Dummy type
    if (nalu.size() <= start_code_len) return 255;
    return (nalu[start_code_len] >> 1) & 0x3F;
}

static void mux_h264_nalus(const std::vector<std::vector<uint8_t>>& nalus, FFmpegMuxer* muxer) {
    std::vector<uint8_t> access_unit;
    int64_t pts = 0;
    int write_count = 0;
    bool is_key = false;

    for (const auto& nalu : nalus) {
        uint8_t type = get_h264_nal_type(nalu);
        if (type == 0) continue;

        access_unit.insert(access_unit.end(), nalu.begin(), nalu.end());
        
        if (type == 5 || type == 7 || type == 8) {
            is_key = true;
        }

        // VCL NALU marks the end of an access unit in a simple stream
        if (type == 1 || type == 5) {
            if (muxer->write_packet(access_unit.data(), access_unit.size(), is_key, pts)) {
                write_count++;
                pts++;
            }
            access_unit.clear();
            is_key = false;
        }
    }
    EXPECT_GT(write_count, 0);
}

static void mux_hevc_nalus(const std::vector<std::vector<uint8_t>>& nalus, FFmpegMuxer* muxer) {
    std::vector<uint8_t> access_unit;
    int64_t pts = 0;
    int write_count = 0;
    bool is_key = false;

    for (const auto& nalu : nalus) {
        uint8_t type = get_h265_nal_type(nalu);
        if (type == 255) continue;

        access_unit.insert(access_unit.end(), nalu.begin(), nalu.end());
        
        if ((type >= 16 && type <= 21) || type == 32 || type == 33 || type == 34) {
            is_key = true;
        }

        // VCL NALUs are 0-31
        if (type <= 31) {
            if (muxer->write_packet(access_unit.data(), access_unit.size(), is_key, pts)) {
                write_count++;
                pts++;
            }
            access_unit.clear();
            is_key = false;
        }
    }
    EXPECT_GT(write_count, 0);
}

class FFmpegMuxerTest : public ::testing::Test {
protected:
    void SetUp() override {
        muxer = std::make_unique<FFmpegMuxer>();
    }

    void TearDown() override {
        if (muxer) {
            muxer->close();
            muxer.reset();
        }
    }

    std::unique_ptr<FFmpegMuxer> muxer;
};

TEST_F(FFmpegMuxerTest, InitAndClose) {
    MuxerConfig config;
    config.output_path = "data/test_empty.mp4";
    config.width = 1280;
    config.height = 720;
    config.fps = 30;
    config.use_hevc = false;

    EXPECT_TRUE(muxer->open(config));
    EXPECT_TRUE(muxer->is_open());
    EXPECT_FALSE(muxer->open(config));
    EXPECT_TRUE(muxer->close());
    EXPECT_FALSE(muxer->is_open());
    EXPECT_TRUE(muxer->close());
}

TEST_F(FFmpegMuxerTest, EdgeCases) {
    uint8_t dummy_data[10] = {0};
    EXPECT_FALSE(muxer->write_packet(dummy_data, 10, true, 0));

    MuxerConfig config;
    config.output_path = "data/test_edge.mp4";
    config.width = 1280;
    config.height = 720;
    config.use_hevc = false;
    
    ASSERT_TRUE(muxer->open(config));
    EXPECT_FALSE(muxer->write_packet(nullptr, 10, true, 0));
    EXPECT_FALSE(muxer->write_packet(dummy_data, 0, true, 0));
    muxer->close();

    MuxerConfig bad_config = config;
    bad_config.output_path = "/root/invalid_dir/no_permission.mp4";
    if (access("/root/invalid_dir", F_OK) == -1) {
        EXPECT_FALSE(muxer->open(bad_config));
    }
}

TEST_F(FFmpegMuxerTest, MuxH264) {
    std::string input_file = "data/out.h264";
    std::string output_file = "data/out_h264.mp4";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found.";
    }

    std::vector<uint8_t> stream_data = read_file(input_file);
    ASSERT_FALSE(stream_data.empty());
    auto nalus = extract_nal_units(stream_data);
    ASSERT_FALSE(nalus.empty());

    MuxerConfig config;
    config.output_path = output_file;
    config.width = 1280;
    config.height = 720;
    config.fps = 25;
    config.use_hevc = false;
    config.faststart = true;
    config.fragmented = false;

    ASSERT_TRUE(muxer->open(config));
    mux_h264_nalus(nalus, muxer.get());
    EXPECT_TRUE(muxer->close());

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
}

TEST_F(FFmpegMuxerTest, MuxHEVC) {
    std::string input_file = "data/out.h265";
    std::string output_file = "data/out_hevc.mp4";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found.";
    }

    std::vector<uint8_t> stream_data = read_file(input_file);
    ASSERT_FALSE(stream_data.empty());
    auto nalus = extract_nal_units(stream_data);
    ASSERT_FALSE(nalus.empty());

    MuxerConfig config;
    config.output_path = output_file;
    config.width = 1280;
    config.height = 720;
    config.fps = 25;
    config.use_hevc = true;
    config.faststart = true;
    config.fragmented = false;

    ASSERT_TRUE(muxer->open(config));
    mux_hevc_nalus(nalus, muxer.get());
    EXPECT_TRUE(muxer->close());

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
}
