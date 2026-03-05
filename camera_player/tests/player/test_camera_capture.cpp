#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "player/camera_capture.hpp"
#include "player/system_interface.hpp"

using ::testing::Return;
using ::testing::_;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::NiceMock;

namespace player {

// =============================================================================
// Mock 系统接口
// =============================================================================

class MockSystemInterface : public SystemInterface {
public:
    MOCK_METHOD(int, open, (const char *pathname, int flags), (override));
    MOCK_METHOD(int, close, (int fd), (override));
    MOCK_METHOD(int, ioctl, (int fd, unsigned long request, void *arg), (override));
    MOCK_METHOD(void*, mmap, (void *addr, size_t length, int prot, int flags, int fd, off_t offset), (override));
    MOCK_METHOD(int, munmap, (void *addr, size_t length), (override));
};

// =============================================================================
// Mock 测试套件 - 逻辑分支覆盖
// =============================================================================

class CameraCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sys_nice = new NiceMock<MockSystemInterface>();
        mock_sys = mock_sys_nice;
        // 默认行为：析构清理不报错
        ON_CALL(*mock_sys_nice, close(_)).WillByDefault(Return(0));
        ON_CALL(*mock_sys_nice, munmap(_, _)).WillByDefault(Return(0));
        ON_CALL(*mock_sys_nice, ioctl(_, VIDIOC_STREAMOFF, _)).WillByDefault(Return(0));
        capture = std::make_unique<CameraCapture>(mock_sys_nice);
    }
    void TearDown() override {
        capture.reset();
        delete mock_sys_nice;
    }
    NiceMock<MockSystemInterface> *mock_sys_nice;
    MockSystemInterface *mock_sys;
    std::unique_ptr<CameraCapture> capture;
    uint8_t dummy_buffer[1024];

    // 辅助：一次完整成功的 open（4 个 buffer）
    void SetupSuccessfulOpen() {
        EXPECT_CALL(*mock_sys, open(StrEq("/dev/video51"), _)).WillOnce(Return(10));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
            .WillOnce(Invoke([](int, unsigned long, void* arg) {
                auto cap = static_cast<struct v4l2_capability*>(arg);
                cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
                return 0;
            }));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _))
            .WillOnce(Invoke([](int, unsigned long, void* arg) {
                auto fmt = static_cast<struct v4l2_format*>(arg);
                fmt->fmt.pix.width = 1280;
                fmt->fmt.pix.height = 720;
                return 0;
            }));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(0));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _))
            .WillOnce(Invoke([](int, unsigned long, void* arg) {
                auto req = static_cast<struct v4l2_requestbuffers*>(arg);
                req->count = 4;
                return 0;
            }));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYBUF, _))
            .Times(4)
            .WillRepeatedly(Invoke([](int, unsigned long, void* arg) {
                auto buf = static_cast<struct v4l2_buffer*>(arg);
                buf->length = 1024;
                return 0;
            }));
        EXPECT_CALL(*mock_sys, mmap(_, 1024, _, _, 10, _))
            .Times(4)
            .WillRepeatedly(Return(dummy_buffer));
        EXPECT_TRUE(capture->open("/dev/video51", 1280, 720, 30));
    }

    // 辅助：成功打开后再启动流
    void SetupSuccessfulStreaming() {
        SetupSuccessfulOpen();
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QBUF, _)).Times(4).WillRepeatedly(Return(0));
        EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_STREAMON, _)).WillOnce(Return(0));
        EXPECT_TRUE(capture->start_streaming());
    }
};

// ----------------------
// open() Happy Path
// ----------------------
TEST_F(CameraCaptureTest, OpenSuccess) {
    SetupSuccessfulOpen();
    EXPECT_EQ(capture->width(), 1280);
    EXPECT_EQ(capture->height(), 720);
    EXPECT_EQ(capture->fps(), 30);
}

// ----------------------
// open() Sad Paths (已覆盖的分支)
// ----------------------
TEST_F(CameraCaptureTest, OpenFail_SysOpen) {
    EXPECT_CALL(*mock_sys, open(StrEq("/dev/video51"), _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_QueryCap) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _)).WillOnce(Return(-1));
    EXPECT_CALL(*mock_sys, close(10)).WillOnce(Return(0));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_NoVideoCaptureCap) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_STREAMING; // 缺 VIDEO_CAPTURE
            return 0;
        }));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_NoStreamingCap) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE; // 缺 STREAMING
            return 0;
        }));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_SetFmt) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

// ----------------------
// 新增边界：S_PARM 失败后仍成功打开，并检查 fps 为 0（除零安全）
// ----------------------
TEST_F(CameraCaptureTest, OpenFail_SetParmFails_CheckFps) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto fmt = static_cast<struct v4l2_format*>(arg);
            fmt->fmt.pix.width = 1280;
            fmt->fmt.pix.height = 720;
            return 0;
        }));
    // S_PARM 失败（返回 -1）
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(-1));
    // 其余成功路径保持不变
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto req = static_cast<struct v4l2_requestbuffers*>(arg);
            req->count = 4;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYBUF, _))
        .Times(4)
        .WillRepeatedly(Invoke([](int, unsigned long, void* arg) {
            auto buf = static_cast<struct v4l2_buffer*>(arg);
            buf->length = 1024;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, mmap(_, 1024, _, _, 10, _))
        .Times(4)
        .WillRepeatedly(Return(dummy_buffer));
    EXPECT_TRUE(capture->open("/dev/video51", 1280, 720, 30));
    // S_PARM 失败是非致命的（仅 warn），open() 仍成功
    // fps() 保持传入的请求值（实现在 S_PARM 之前就设置了 fps_）
    EXPECT_EQ(capture->fps(), 30);
}

// ----------------------
// OpenFail_ReqBufs、OpenFail_QueryBuf、OpenFail_MMapFail 已有实现（保持不变）
// ----------------------
TEST_F(CameraCaptureTest, OpenFail_ReqBufs) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_QueryBuf) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto req = static_cast<struct v4l2_requestbuffers*>(arg);
            req->count = 4;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYBUF, _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

TEST_F(CameraCaptureTest, OpenFail_MMapFail) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto req = static_cast<struct v4l2_requestbuffers*>(arg);
            req->count = 4;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYBUF, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, mmap(_, _, _, _, _, _)).WillOnce(Return(MAP_FAILED));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
}

// ----------------------
// start_streaming、capture_frame、close 边界
// ----------------------
TEST_F(CameraCaptureTest, StreamingFlow) {
    SetupSuccessfulStreaming();
    // 再次调用 start_streaming 应该直接返回 true（幂等）
    EXPECT_TRUE(capture->start_streaming());
    // capture_frame
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_DQBUF, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto buf = static_cast<struct v4l2_buffer*>(arg);
            buf->index = 0;
            buf->bytesused = 512;
            return 0;
        }));
    uint8_t *data = nullptr;
    size_t size = 0;
    EXPECT_TRUE(capture->capture_frame(&data, &size));
    EXPECT_EQ(data, dummy_buffer);
    EXPECT_EQ(size, 512);
    // release_frame
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QBUF, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto buf = static_cast<struct v4l2_buffer*>(arg);
            EXPECT_EQ(buf->index, 0);
            return 0;
        }));
    capture->release_frame();
    // 再次 release_frame（无帧）不应产生 ioctl
    capture->release_frame();
    // stop_streaming
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_STREAMOFF, _)).WillOnce(Return(0));
    capture->stop_streaming();
    // 再次 stop_streaming 幂等
    capture->stop_streaming();
}

TEST_F(CameraCaptureTest, StartStreaming_QBufFail) {
    SetupSuccessfulOpen();
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QBUF, _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->start_streaming());
}

TEST_F(CameraCaptureTest, StartStreaming_StreamOnFail) {
    SetupSuccessfulOpen();
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QBUF, _)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_STREAMON, _)).WillOnce(Return(-1));
    EXPECT_FALSE(capture->start_streaming());
}

TEST_F(CameraCaptureTest, CaptureFrame_NotStreaming) {
    SetupSuccessfulOpen();
    uint8_t *data = nullptr;
    size_t size = 0;
    EXPECT_FALSE(capture->capture_frame(&data, &size));
}

TEST_F(CameraCaptureTest, CaptureFrame_DQBufFail) {
    SetupSuccessfulStreaming();
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_DQBUF, _)).WillOnce(Return(-1));
    uint8_t *data = nullptr;
    size_t size = 0;
    EXPECT_FALSE(capture->capture_frame(&data, &size));
    // 析构时会调用 stop_streaming，预期一次 STREAMOFF
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_STREAMOFF, _)).WillOnce(Return(0));
}

// ----------------------
// close() 边界：mmap 失败后 close（不调用 munmap）
// ----------------------
TEST_F(CameraCaptureTest, CloseWhenMmapFailed) {
    // 模拟 mmap 失败的 open 流程
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYCAP, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto cap = static_cast<struct v4l2_capability*>(arg);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_FMT, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_S_PARM, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_REQBUFS, _))
        .WillOnce(Invoke([](int, unsigned long, void* arg) {
            auto req = static_cast<struct v4l2_requestbuffers*>(arg);
            req->count = 1;
            return 0;
        }));
    EXPECT_CALL(*mock_sys, ioctl(10, VIDIOC_QUERYBUF, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_sys, mmap(_, _, _, _, _, _)).WillOnce(Return(MAP_FAILED));
    // open 失败，后续只会调用 close
    EXPECT_CALL(*mock_sys, close(10)).WillOnce(Return(0));
    EXPECT_FALSE(capture->open("/dev/video51", 1280, 720, 30));
    // 手动调用 close（已在析构中调用），确保不会触发 munmap
    capture->close();
}

// ----------------------
// 初始状态、无操作安全性
// ----------------------
TEST_F(CameraCaptureTest, InitialState) {
    EXPECT_EQ(capture->width(), 0);
    EXPECT_EQ(capture->height(), 0);
    EXPECT_EQ(capture->fps(), 0);
}

TEST_F(CameraCaptureTest, CloseWithoutOpen) {
    capture->close();
    capture->close(); // 再次调用安全
}

TEST_F(CameraCaptureTest, CloseIdempotency) {
    SetupSuccessfulOpen();
    EXPECT_CALL(*mock_sys, munmap(dummy_buffer, 1024)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_sys, close(10)).WillOnce(Return(0));
    capture->close();
    // 第二次 close 不应再调用系统函数
    capture->close();
}

// =============================================================================
// 真实设备测试套件（保持原有实现）
// =============================================================================

class CameraCaptureRealDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (access("/dev/video51", F_OK) != 0) {
            GTEST_SKIP() << "设备 /dev/video51 不存在";
        }
        // 检查是否具备 VIDEO_CAPTURE 能力
        int fd = ::open("/dev/video51", O_RDWR);
        if (fd < 0) {
            GTEST_SKIP() << "无法打开 /dev/video51，可能权限不足";
        }
        struct v4l2_capability cap = {};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            ::close(fd);
            GTEST_SKIP() << "设备不支持视频采集 (V4L2_CAP_VIDEO_CAPTURE)";
        }
        ::close(fd);
    }
    CameraCapture capture; // 默认 LinuxSystem
};

TEST_F(CameraCaptureRealDeviceTest, OpenRealDevice) {
    EXPECT_TRUE(capture.open("/dev/video51", 1280, 720, 30));
    EXPECT_GT(capture.width(), 0);
    EXPECT_GT(capture.height(), 0);
    EXPECT_GT(capture.fps(), 0);
}

TEST_F(CameraCaptureRealDeviceTest, FullStreamingCycle) {
    ASSERT_TRUE(capture.open("/dev/video51", 1280, 720, 30));
    ASSERT_TRUE(capture.start_streaming());
    uint8_t *data = nullptr;
    size_t size = 0;
    ASSERT_TRUE(capture.capture_frame(&data, &size));
    EXPECT_NE(data, nullptr);
    EXPECT_GT(size, 0);
    capture.release_frame();
    capture.stop_streaming();
}

TEST_F(CameraCaptureRealDeviceTest, MultiFrameCapture) {
    ASSERT_TRUE(capture.open("/dev/video51", 1280, 720, 30));
    ASSERT_TRUE(capture.start_streaming());
    for (int i = 0; i < 10; ++i) {
        uint8_t *data = nullptr;
        size_t size = 0;
        ASSERT_TRUE(capture.capture_frame(&data, &size)) << "第 " << i << " 帧采集失败";
        EXPECT_NE(data, nullptr);
        EXPECT_GT(size, 0);
        capture.release_frame();
    }
    capture.stop_streaming();
}

TEST_F(CameraCaptureRealDeviceTest, OpenInvalidDevice) {
    EXPECT_FALSE(capture.open("/dev/video_nonexistent", 1280, 720, 30));
}

} // namespace player
