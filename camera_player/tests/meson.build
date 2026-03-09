# Google Test 依赖
gtest_dep = dependency('gtest', required: true, static: true)
gmock_dep = dependency('gmock', required: true, static: true)

# 测试源文件
test_sources = files(
    'test_main.cpp',
    'player/test_camera_capture.cpp',
    'player/test_mpp_decode.cpp',
    'player/test_mpp_encode.cpp',
)

# 源码文件 (从父目录引用并过滤掉 main.cpp 相关的)
# 这里直接引用 player_sources 相关的实现
test_player_sources = files(
    '../src/player/camera_capture.cpp',
    '../src/player/system_interface.cpp',
    '../src/player/mpp_decode.cpp',
    '../src/player/mpp_encode.cpp',
    '../src/player/ffmpeg_muxer.cpp',
)

# 测试包含目录
test_inc_dirs = include_directories('.')

# 定义测试可执行文件
test_exe = executable('camera_player_tests',
    test_sources,
    test_player_sources,
    include_directories: [system_inc_dirs, user_inc_dirs, test_inc_dirs],
    dependencies: [
        gtest_dep,
        gmock_dep,
        #qt5_core,
        #qt5_gui,
        #qt5_widgets,
        #libdrm_dep,
        #librga_dep,
        mpp_dep,
        avcodec_dep,
        avformat_dep,
        avutil_dep,
        spdlog_dep,
    ],
)

# 注册测试到 meson test
test('camera_player_tests', test_exe)
