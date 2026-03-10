#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "player/ffmpeg_demuxer.hpp"

using namespace player;

class FFmpegDemuxerTest : public ::testing::Test {
protected:
    void SetUp() override {
        demuxer = std::make_unique<FFmpegDemuxer>();
    }

    void TearDown() override {
        if (demuxer) {
            demuxer->close();
            demuxer.reset();
        }
    }

    std::unique_ptr<FFmpegDemuxer> demuxer;
};

TEST_F(FFmpegDemuxerTest, OpenAndClose) {
    std::string input_file = "data/out_h264.mp4";
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found. Run FFmpegMuxer tests first.";
    }

    EXPECT_TRUE(demuxer->open(input_file));
    EXPECT_GT(demuxer->get_width(), 0);
    EXPECT_GT(demuxer->get_height(), 0);
    EXPECT_GE(demuxer->get_fps(), 0.0);
    EXPECT_FALSE(demuxer->is_hevc());

    demuxer->close();
    
    // Test re-open
    EXPECT_TRUE(demuxer->open(input_file));
    demuxer->close();
}

TEST_F(FFmpegDemuxerTest, EdgeCases) {
    // 1. Open non-existent file
    EXPECT_FALSE(demuxer->open("data/non_existent.mp4"));

    // 2. Read packet before open
    std::vector<uint8_t> data;
    int64_t pts;
    EXPECT_FALSE(demuxer->read_packet(data, pts));

    // 3. Seek before open
    EXPECT_FALSE(demuxer->seek(1.0));

    // 4. Double open
    std::string input_file = "data/out_h264.mp4";
    if (access(input_file.c_str(), F_OK) == 0) {
        ASSERT_TRUE(demuxer->open(input_file));
        EXPECT_FALSE(demuxer->open(input_file));
    }
}

TEST_F(FFmpegDemuxerTest, DemuxH264) {
    std::string input_file = "data/out_h264.mp4";
    std::string output_file = "data/out_demux.h264";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found.";
    }

    ASSERT_TRUE(demuxer->open(input_file));
    EXPECT_FALSE(demuxer->is_hevc());

    std::ofstream ofs(output_file, std::ios::binary);
    ASSERT_TRUE(ofs.is_open());

    std::vector<uint8_t> data;
    int64_t pts;
    int frame_count = 0;

    while (demuxer->read_packet(data, pts)) {
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        frame_count++;
    }

    EXPECT_GT(frame_count, 0);
    ofs.close();

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
    
    printf("Demuxed %d H.264 frames to %s\n", frame_count, output_file.c_str());
}

TEST_F(FFmpegDemuxerTest, DemuxHEVC) {
    std::string input_file = "data/out_hevc.mp4";
    std::string output_file = "data/out_demux.h265";
    
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found.";
    }

    ASSERT_TRUE(demuxer->open(input_file));
    EXPECT_TRUE(demuxer->is_hevc());

    std::ofstream ofs(output_file, std::ios::binary);
    ASSERT_TRUE(ofs.is_open());

    std::vector<uint8_t> data;
    int64_t pts;
    int frame_count = 0;

    while (demuxer->read_packet(data, pts)) {
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        frame_count++;
    }

    EXPECT_GT(frame_count, 0);
    ofs.close();

    struct stat st;
    EXPECT_EQ(stat(output_file.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
    
    printf("Demuxed %d HEVC frames to %s\n", frame_count, output_file.c_str());
}

TEST_F(FFmpegDemuxerTest, SeekTest) {
    std::string input_file = "data/out_h264.mp4";
    if (access(input_file.c_str(), F_OK) != 0) {
        GTEST_SKIP() << "Input file " << input_file << " not found.";
    }

    ASSERT_TRUE(demuxer->open(input_file));
    
    // Seek to 0.5s if duration permits, else just seek to 0
    double seek_time = (demuxer->get_duration_ms() > 1000) ? 0.5 : 0.0;
    
    EXPECT_TRUE(demuxer->seek(seek_time));
    
    std::vector<uint8_t> data;
    int64_t pts;
    EXPECT_TRUE(demuxer->read_packet(data, pts));
    EXPECT_GT(data.size(), 0);
}
