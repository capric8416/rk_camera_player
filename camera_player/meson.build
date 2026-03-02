
# Camera Player

project('camera_player', ['c', 'cpp'],
    version: '1.0.0',
    default_options: [
        'c_std=c11',
        'cpp_std=c++17',
        'warning_level=2',
        'buildtype=release',
    ]
)

# Qt5 依赖
qt5_core = dependency('Qt5Core', required: true)
qt5_gui = dependency('Qt5Gui', required: true)
qt5_widgets = dependency('Qt5Widgets', required: true)

# ffmpeg 依赖
avcodec_dep = dependency('libavcodec', required: true)
avformat_dep = dependency('libavformat', required: true)
avutil_dep = dependency('libavutil', required: true)

# rockchip 依赖
libdrm_dep = dependency('libdrm', required: true)
librga_dep = dependency('librga', required: true)
mpp_dep = dependency('rockchip_mpp', required: true)

# spdlog 依赖
spdlog_dep = dependency('spdlog', required: true)

# Qt5 模块
qt5 = import('qt5')

# 包含目录
# 包含目录
system_inc_dirs = include_directories(
    '../third_party/include',
    '../third_party/include/qt5',
    '../third_party/include/qt5/QtCore',
    '../third_party/include/qt5/QtGui',
    '../third_party/include/qt5/QtWidgets',
    '../third_party/include/libdrm',
    is_system: true,
)

user_inc_dirs = include_directories(
    'src',
    'src/player',
)

# GUI 源文件
gui_sources = files(
    'src/main.cpp',
    'src/main_window.cpp',
    'src/player_thread.cpp',
    'src/stdin_cmd.cpp',
    'src/camera_overlay_page.cpp',
    'src/file_overlay_page.cpp',
    'src/launcher_page.cpp',
    'src/gallery_page.cpp',
    'src/videos_page.cpp',
    'src/settings_page.cpp',
    'src/languages_page.cpp',
    'src/change_date_time_page.cpp',
    'src/rolling_box.cpp',
    'src/rolling_box_y.cpp',
)

# Player 源文件 (Camera Overlay)
player_sources = files(
    'src/player/camera_player.cpp',
    'src/player/file_player.cpp',
    'src/player/camera_capture.cpp',
    'src/player/mpp_decode.cpp',
    'src/player/mpp_encode.cpp',
    'src/player/ffmpeg_muxer.cpp',
    'src/player/ffmpeg_demuxer.cpp',
    'src/player/rga_scale.cpp',
    'src/player/drm_display.cpp',
)

# 头文件 (需要 MOC 处理)
headers = files(
    'src/main_window.hpp',
    'src/player_thread.hpp',
    'src/stdin_cmd.hpp',
    'src/camera_overlay_page.hpp',
    'src/file_overlay_page.hpp',
    'src/launcher_page.hpp',
    'src/gallery_page.hpp',
    'src/videos_page.hpp',
    'src/settings_page.hpp',
    'src/languages_page.hpp',
    'src/change_date_time_page.hpp',
    'src/rolling_box.hpp',
    'src/rolling_box_y.hpp',
)

# UI 文件
ui_files = files(
    'ui/main_window.ui',
    'ui/camera_overlay_page.ui',
    'ui/file_overlay_page.ui',
    'ui/launcher_page.ui',
    'ui/gallery_page.ui',
    'ui/videos_page.ui',
    'ui/settings_page.ui',
    'ui/languages_page.ui',
    'ui/change_date_time_page.ui',
)

# Qt5 预处理
# MOC: 生成 moc_*.cpp
moc_files = qt5.compile_moc(
    headers: headers,
    dependencies: [qt5_core, qt5_gui, qt5_widgets],
)

# UIC: 生成 ui_*.h
ui_headers = qt5.compile_ui(
    sources: ui_files,
)

# RCC: 编译资源文件 (.qrc -> .cpp)
qrc_files = files('resources.qrc')
rcc_files = qt5.compile_resources(
    sources: qrc_files,
)

# 注意: 翻译文件使用 build.py translate 命令编译 (.ts -> .qm)

# 编译可执行文件
executable('camera_player',
    gui_sources,
    player_sources,
    moc_files,
    ui_headers,
    rcc_files,
    include_directories: [system_inc_dirs, user_inc_dirs],
    dependencies: [
        qt5_core,
        qt5_gui,
        qt5_widgets,
        libdrm_dep,
        librga_dep,
        mpp_dep,
        avcodec_dep,
        avformat_dep,
        avutil_dep,
        spdlog_dep,
    ],
    install: true,
)
