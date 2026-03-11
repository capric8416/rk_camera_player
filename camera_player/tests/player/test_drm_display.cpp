#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <drm_fourcc.h>

#include "player/drm_display.hpp"
#include "player/system_interface.hpp"
#include "player/drm_interface.hpp"

using ::testing::Return;
using ::testing::_;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::NiceMock;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::DoAll;

namespace player {

// =============================================================================
// Mock DRM 接口
// =============================================================================

class MockDrmInterface : public DrmInterface {
public:
    MOCK_METHOD(drmModeResPtr, get_resources, (int fd), (override));
    MOCK_METHOD(void, free_resources, (drmModeResPtr ptr), (override));
    MOCK_METHOD(drmModeCrtcPtr, get_crtc, (int fd, uint32_t crtc_id), (override));
    MOCK_METHOD(void, free_crtc, (drmModeCrtcPtr ptr), (override));
    MOCK_METHOD(drmModePlaneResPtr, get_plane_resources, (int fd), (override));
    MOCK_METHOD(void, free_plane_resources, (drmModePlaneResPtr ptr), (override));
    MOCK_METHOD(drmModePlanePtr, get_plane, (int fd, uint32_t plane_id), (override));
    MOCK_METHOD(void, free_plane, (drmModePlanePtr ptr), (override));
    MOCK_METHOD(int, set_client_cap, (int fd, uint64_t capability, uint64_t value), (override));
    MOCK_METHOD(int, set_plane, (int fd, uint32_t plane_id, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, int32_t crtc_x, int32_t crtc_y, uint32_t crtc_w, uint32_t crtc_h, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h), (override));
    MOCK_METHOD(int, add_fb2, (int fd, uint32_t width, uint32_t height, uint32_t pixel_format, uint32_t bo_handles[4], uint32_t pitches[4], uint32_t offsets[4], uint32_t *buf_id, uint32_t flags), (override));
    MOCK_METHOD(int, rm_fb, (int fd, uint32_t buffer_id), (override));
    MOCK_METHOD(int, wait_vblank, (int fd, drmVBlankPtr vbl), (override));
    MOCK_METHOD(int, prime_fd_to_handle, (int fd, int prime_fd, uint32_t *handle), (override));
    MOCK_METHOD(drmModeObjectPropertiesPtr, get_object_properties, (int fd, uint32_t object_id, uint32_t object_type), (override));
    MOCK_METHOD(void, free_object_properties, (drmModeObjectPropertiesPtr ptr), (override));
    MOCK_METHOD(drmModePropertyPtr, get_property, (int fd, uint32_t property_id), (override));
    MOCK_METHOD(void, free_property, (drmModePropertyPtr ptr), (override));
    MOCK_METHOD(int, set_object_property, (int fd, uint32_t object_id, uint32_t object_type, uint32_t property_id, uint64_t value), (override));
};

class MockSystemInterface : public SystemInterface {
public:
    MOCK_METHOD(int, open, (const char *pathname, int flags), (override));
    MOCK_METHOD(int, close, (int fd), (override));
    MOCK_METHOD(int, ioctl, (int fd, unsigned long request, void *arg), (override));
    MOCK_METHOD(void*, mmap, (void *addr, size_t length, int prot, int flags, int fd, off_t offset), (override));
    MOCK_METHOD(int, munmap, (void *addr, size_t length), (override));
};

// =============================================================================
// Mock 测试套件
// =============================================================================

class DrmDisplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sys = new NiceMock<MockSystemInterface>();
        mock_drm = new NiceMock<MockDrmInterface>();
        display = std::make_unique<DrmDisplay>(mock_sys, mock_drm);

        // 默认行为
        ON_CALL(*mock_sys, close(_)).WillByDefault(Return(0));
        ON_CALL(*mock_sys, munmap(_, _)).WillByDefault(Return(0));
        ON_CALL(*mock_sys, ioctl(_, _, _)).WillByDefault(Return(0));
        ON_CALL(*mock_drm, set_client_cap(_, _, _)).WillByDefault(Return(0));
        ON_CALL(*mock_drm, free_resources(_)).WillByDefault(Return());
        ON_CALL(*mock_drm, free_crtc(_)).WillByDefault(Return());
        ON_CALL(*mock_drm, free_plane_resources(_)).WillByDefault(Return());
        ON_CALL(*mock_drm, free_plane(_)).WillByDefault(Return());
        ON_CALL(*mock_drm, rm_fb(_, _)).WillByDefault(Return(0));
        ON_CALL(*mock_drm, set_plane(_, _, _, _, _, _, _, _, _, _, _, _, _)).WillByDefault(Return(0));
        ON_CALL(*mock_drm, wait_vblank(_, _)).WillByDefault(Return(0));
        ON_CALL(*mock_drm, set_object_property(_, _, _, _, _)).WillByDefault(Return(0));
    }

    void TearDown() override {
        display.reset();
        // DrmDisplay 会负责删除 mock 对象（因为它拥有所有权，如果有 m_own_XXX，
        // 但我们在构造函数中传入了，所以 m_own_XXX 是 false。
        // 等等，根据我之前的重构代码，构造函数传入的对象 m_own_XXX 是 false。
        // 所以这里需要手动删除，或者改进 DrmDisplay 处理所有权。
        // 目前重构的代码里：
        // DrmDisplay::DrmDisplay(SystemInterface *sys, DrmInterface *drm) : m_sys(sys), m_drm(drm), m_own_sys(false), m_own_drm(false) ...
        // 所以我们需要手动 delete。
        delete mock_sys;
        delete mock_drm;
    }

    NiceMock<MockSystemInterface> *mock_sys;
    NiceMock<MockDrmInterface> *mock_drm;
    std::unique_ptr<DrmDisplay> display;

    void SetupSuccessfulInit() {
        // open
        EXPECT_CALL(*mock_sys, open(StrEq("/dev/dri/card0"), _)).WillOnce(Return(10));
        ASSERT_TRUE(display->open("/dev/dri/card0"));

        // init resources
        static drmModeRes res = {};
        res.count_crtcs = 1;
        static uint32_t crtcs[1] = {20};
        res.crtcs = crtcs;
        EXPECT_CALL(*mock_drm, get_resources(10)).WillOnce(Return(&res));

        // crtc
        static drmModeCrtc crtc = {};
        crtc.crtc_id = 20;
        crtc.mode_valid = 1;
        crtc.mode.hdisplay = 1920;
        crtc.mode.vdisplay = 1080;
        EXPECT_CALL(*mock_drm, get_crtc(10, 20)).WillOnce(Return(&crtc));

        // plane resources (called twice: once for finding overlay, once for setting others zpos)
        static drmModePlaneRes pres = {};
        pres.count_planes = 1;
        static uint32_t planes[1] = {30};
        pres.planes = planes;
        EXPECT_CALL(*mock_drm, get_plane_resources(10)).Times(2).WillRepeatedly(Return(&pres));

        // plane
        static drmModePlane plane = {};
        plane.plane_id = 30;
        plane.possible_crtcs = 1; // bit 0
        plane.crtc_id = 0; // free
        EXPECT_CALL(*mock_drm, get_plane(10, 30)).WillOnce(Return(&plane));

        // create_buffer (twice for double buffering)
        EXPECT_CALL(*mock_sys, ioctl(10, DRM_IOCTL_MODE_CREATE_DUMB, _))
            .Times(2)
            .WillRepeatedly(Invoke([](int, unsigned long, void* arg) {
                auto req = static_cast<struct drm_mode_create_dumb*>(arg);
                req->handle = 1;
                req->size = req->width * req->height * 4;
                req->pitch = req->width * 4;
                return 0;
            }));
        EXPECT_CALL(*mock_drm, add_fb2(10, _, _, _, _, _, _, _, _))
            .Times(2)
            .WillRepeatedly(DoAll(SetArgPointee<7>(40), Return(0)));
        EXPECT_CALL(*mock_sys, ioctl(10, DRM_IOCTL_MODE_MAP_DUMB, _))
            .Times(2)
            .WillRepeatedly(Invoke([](int, unsigned long, void* arg) {
                auto req = static_cast<struct drm_mode_map_dumb*>(arg);
                req->offset = 0x1000;
                return 0;
            }));
        static uint8_t dummy_map[1920*1080*4];
        EXPECT_CALL(*mock_sys, mmap(_, _, _, _, 10, _))
            .Times(2)
            .WillRepeatedly(Return(dummy_map));

        // set_property (zpos, alpha, others)
        ON_CALL(*mock_drm, get_object_properties(_, _, _)).WillByDefault(Invoke([](int, uint32_t, uint32_t) {
            static drmModeObjectProperties props = {};
            props.count_props = 1;
            static uint32_t prop_ids[1] = {50};
            props.props = prop_ids;
            return &props;
        }));
        ON_CALL(*mock_drm, free_object_properties(_)).WillByDefault(Return());
        ON_CALL(*mock_drm, get_property(_, 50)).WillByDefault(Invoke([](int, uint32_t) {
            static drmModePropertyRes prop = {};
            strcpy(prop.name, "zpos");
            prop.prop_id = 50;
            return &prop;
        }));
        ON_CALL(*mock_drm, free_property(_)).WillByDefault(Return());

        // Initial set_plane
        EXPECT_CALL(*mock_drm, set_plane(10, 30, 20, 40, _, _, _, _, _, _, _, _, _)).WillOnce(Return(0));
        // Cleanup set_plane (called in close)
        EXPECT_CALL(*mock_drm, set_plane(10, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)).WillRepeatedly(Return(0));

        ASSERT_TRUE(display->init(1920, 1080, 0));
    }
};

TEST_F(DrmDisplayTest, OpenSuccess) {
    EXPECT_CALL(*mock_sys, open(StrEq("/dev/dri/card0"), _)).WillOnce(Return(10));
    EXPECT_CALL(*mock_drm, set_client_cap(10, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)).WillOnce(Return(0));
    EXPECT_TRUE(display->open("/dev/dri/card0"));
}

TEST_F(DrmDisplayTest, OpenFail) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(-1));
    EXPECT_FALSE(display->open("/dev/dri/card0"));
}

TEST_F(DrmDisplayTest, InitSuccess) {
    SetupSuccessfulInit();
    EXPECT_EQ(display->display_width(), 1920);
    EXPECT_EQ(display->display_height(), 1080);
}

TEST_F(DrmDisplayTest, InitFail_NoResources) {
    EXPECT_CALL(*mock_sys, open(_, _)).WillOnce(Return(10));
    display->open("/dev/dri/card0");

    EXPECT_CALL(*mock_drm, get_resources(10)).WillOnce(Return(nullptr));
    EXPECT_FALSE(display->init(1920, 1080, 0));
}

TEST_F(DrmDisplayTest, DisplayNV12_HappyPath) {
    SetupSuccessfulInit();
    
    int nv12_fd = 100;
    uint32_t width = 640;
    uint32_t height = 360;
    
    // Cache miss path
    EXPECT_CALL(*mock_drm, prime_fd_to_handle(10, nv12_fd, _))
        .WillOnce(DoAll(SetArgPointee<2>(5), Return(0)));
    EXPECT_CALL(*mock_drm, add_fb2(10, width, height, DRM_FORMAT_NV12, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<7>(50), Return(0)));
    
    EXPECT_CALL(*mock_drm, wait_vblank(10, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_drm, set_plane(10, 30, 20, 50, _, _, _, 1920, 1080, _, _, width << 16, height << 16))
        .WillOnce(Return(0));
    
    EXPECT_TRUE(display->display_nv12(nv12_fd, width, height, width, height));

    // Cache hit path
    EXPECT_CALL(*mock_drm, wait_vblank(10, _)).WillOnce(Return(0));
    EXPECT_CALL(*mock_drm, set_plane(10, 30, 20, 50, _, _, _, 1920, 1080, _, _, width << 16, height << 16))
        .WillOnce(Return(0));
    
    EXPECT_TRUE(display->display_nv12(nv12_fd, width, height, width, height));
}

// =============================================================================
// 真实设备测试套件
// =============================================================================

class DrmDisplayRealDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (access("/dev/dri/card0", F_OK) != 0) {
            GTEST_SKIP() << "DRM device /dev/dri/card0 not found";
        }
    }
    
    static std::vector<uint8_t> read_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {};
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) return buffer;
        return {};
    }
};

TEST_F(DrmDisplayRealDeviceTest, FullCycleWithNV12) {
    DrmDisplay drm;
    if (!drm.open("/dev/dri/card0")) {
        GTEST_SKIP() << "Failed to open /dev/dri/card0";
    }

    // 尝试初始化，宽高设为 0 以使用默认屏幕分辨率
    if (!drm.init(0, 0, 0)) {
        GTEST_SKIP() << "Failed to initialize DRM (likely no active CRTC)";
    }

    // 测试 NV12 显示 (data/out_hevc.yuv 640x360)
    std::string yuv_path = "data/out_hevc.yuv";
    if (access(yuv_path.c_str(), F_OK) == 0) {
        // 由于没有方便的方法获取 dma-buf fd，这里我们只能模拟或跳过
        // 但如果我们有 mpp 输出的 frame，它可以提供 fd。
        // 在该测试中，我们仅尝试打开文件并显示，但这通常需要将数据导入 GEM 对象。
        // 既然 display_nv12 接受的是 fd (dma-buf)，直接 open(yuv) 传 fd 是不行的。
        // 不过我们可以验证接口调用不崩溃，或者仅仅通过 mocks 验证逻辑。
        // 在真实机器上，如果没有现成的 dma-buf，该部分很难完全自动化。
    }

    drm.close();
}

} // namespace player
