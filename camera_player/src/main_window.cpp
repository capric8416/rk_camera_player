#include "main_window.hpp"
#include "ui_main_window.h"
#include "launcher_page.hpp"
#include "camera_overlay_page.hpp"
#include "file_overlay_page.hpp"
#include "gallery_page.hpp"
#include "videos_page.hpp"
#include "stdin_cmd.hpp"
#include "player/logger.hpp"
#include "player/file_player.hpp"
#include <QFile>
#include <QDir>
#include <QLocale>
#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>

static const QString STYLE_DIR = ":/resources/style";
static const QString TRANSLATIONS_DIR = ":/translations/";

// ============================================================================
// MainWindow 实现
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_translator(new QTranslator(this))
    , m_camera_check_timer(new QTimer(this))
    , m_playback_timer(new QTimer(this))
    , m_player_thread(new PlayerThread(this))
    , m_file_player_thread(nullptr)
    , m_stdin_handler(nullptr)
    , m_current_video_index(-1)
    , m_is_playing(false)
{
    // 加载翻译文件
    load_translation();
    
    ui->setupUi(this);
    
    // 无边框全屏显示
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    move(0, 0);
    showFullScreen();
    
    // 设置透明背景，让摄像头画面透过来
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 从外部文件加载样式表
    load_style_sheet();
    
    // 连接相机信号
    connect(m_player_thread, &PlayerThread::camera_connected,
            this, &MainWindow::handle_camera_connected);
    connect(m_player_thread, &PlayerThread::camera_disconnected,
            this, &MainWindow::handle_camera_disconnected);
    
    // 连接 launcher 页面 back 按钮点击信号，启动相机
    connect(ui->launcherPage, &LauncherPage::back_clicked,
            this, &MainWindow::handle_back_clicked);
    
    // 连接 launcher 页面 gallery 按钮点击信号
    connect(ui->launcherPage, &LauncherPage::gallery_clicked,
            this, &MainWindow::show_gallery_page);
    connect(ui->launcherPage, &LauncherPage::videos_clicked,
            this, &MainWindow::show_videos_page);
    
    // 连接 gallery 页面返回信号
    connect(ui->galleryPage, &GalleryPage::back_clicked,
            this, &MainWindow::show_launcher_page);
            
    // 连接 videos 页面信号
    connect(ui->videosPage, &VideosPage::back_clicked,
            this, &MainWindow::show_launcher_page);
    connect(ui->videosPage, &VideosPage::video_selected,
            this, &MainWindow::start_video_playback);
    
    // 连接 camera overlay 页面信号
    connect(ui->cameraOverlayPage, &CameraOverlayPage::capture_image_requested,
            this, &MainWindow::capture_image);
    connect(ui->cameraOverlayPage, &CameraOverlayPage::start_video_requested,
            this, &MainWindow::start_video_capture);
    connect(ui->cameraOverlayPage, &CameraOverlayPage::stop_video_requested,
            this, &MainWindow::stop_video_capture);
    
    // 连接 file overlay 页面信号
    connect(ui->fileOverlayPage, &FileOverlayPage::back_clicked,
            this, &MainWindow::stop_video_playback);
    connect(ui->fileOverlayPage, &FileOverlayPage::toggle_play_pause,
            this, &MainWindow::toggle_video_play_pause);
    connect(ui->fileOverlayPage, &FileOverlayPage::previous_video_requested,
            this, &MainWindow::play_previous_video);
    connect(ui->fileOverlayPage, &FileOverlayPage::next_video_requested,
            this, &MainWindow::play_next_video);
    connect(ui->fileOverlayPage, &FileOverlayPage::delete_video_requested,
            this, &MainWindow::delete_current_video);
    
    // 播放时间更新定时器
    connect(m_playback_timer, &QTimer::timeout,
            this, &MainWindow::update_playback_time);
    
    // 设置 stdin 命令处理器
    m_stdin_handler = new StdinCommandHandler(this, this);
    
    // 初始显示 launcher 页面，不自动启动相机
    show_launcher_page();
    
    LOG_INFO("MainWindow created, size: {}x{}", size().width(), size().height());
}

MainWindow::~MainWindow()
{
    stop_camera();
    stop_video_playback();
    delete ui;
}

void MainWindow::show_camera_overlay_page()
{
    ui->stackedWidget->setCurrentWidget(ui->cameraOverlayPage);
    LOG_INFO("Switched to camera overlay page");
}

void MainWindow::show_file_overlay_page()
{
    ui->stackedWidget->setCurrentWidget(ui->fileOverlayPage);
    LOG_INFO("Switched to file overlay page");
}

void MainWindow::show_launcher_page()
{
    ui->stackedWidget->setCurrentWidget(ui->launcherPage);
    LOG_INFO("Switched to launcher page");
}

void MainWindow::show_gallery_page()
{
    // 设置图片目录并刷新
    ui->galleryPage->set_images_directory("images");
    ui->galleryPage->refresh();
    ui->stackedWidget->setCurrentWidget(ui->galleryPage);
    LOG_INFO("Switched to gallery page");
}

void MainWindow::show_videos_page()
{
    // 设置视频目录并刷新
    ui->videosPage->set_videos_directory("videos");
    ui->videosPage->refresh();
    ui->stackedWidget->setCurrentWidget(ui->videosPage);
    LOG_INFO("Switched to videos page");
}

void MainWindow::handle_camera_connected()
{
    LOG_INFO("Camera connected, showing overlay page");
    show_camera_overlay_page();
}

void MainWindow::handle_camera_disconnected()
{
    LOG_INFO("Camera disconnected, showing launcher page");
    show_launcher_page();
}

void MainWindow::handle_back_clicked()
{
    LOG_INFO("Back clicked, starting camera...");
    start_camera();
}

void MainWindow::check_camera_status()
{
    // 如果相机线程没有运行且不在捕获中，尝试重新连接
    if (!m_player_thread->isRunning() && !m_player_thread->is_capturing()) {
        LOG_DEBUG("Camera not running, attempting to reconnect...");
        start_camera();
    }
}

void MainWindow::capture_image()
{
    LOG_INFO("Capture image requested via command");
    
    QString filepath = m_player_thread->capture_image();
    if (!filepath.isEmpty()) {
        LOG_INFO("Image captured: {}", filepath.toStdString());
        ui->cameraOverlayPage->set_file_path(filepath);
    }
}

void MainWindow::start_video_capture()
{
    LOG_INFO("Start video capture requested");
    
    if (m_player_thread->start_video_capture()) {
        LOG_INFO("Video capture started");
    } else {
        LOG_WARN("Failed to start video capture");
    }
}

void MainWindow::stop_video_capture()
{
    LOG_INFO("Stop video capture requested");
    
    QString filepath = m_player_thread->stop_video_capture();
    if (!filepath.isEmpty()) {
        LOG_INFO("Video saved: {}", filepath.toStdString());
        ui->cameraOverlayPage->set_file_path(filepath);
    }
}

double MainWindow::parse_fps_from_meta(const QString &video_path)
{
    // 从视频路径找到对应的 meta 文件并解析 fps
    // meta 格式: {basename}_{fps}_{frames}.jpeg
    QString meta_dir = QFileInfo(video_path).path() + "/.meta";
    QString video_basename = QFileInfo(video_path).completeBaseName();
    
    QDir dir(meta_dir);
    QStringList filters;
    filters << video_basename + "_*.jpeg";
    QStringList meta_files = dir.entryList(filters, QDir::Files);
    
    if (meta_files.isEmpty()) {
        LOG_WARN("Meta file not found for: {}", video_path.toStdString());
        return 30.0;
    }
    
    // 解析 fps: {basename}_{fps}_{frames}.jpeg
    QString meta_name = QFileInfo(meta_files.first()).completeBaseName();
    QStringList parts = meta_name.split('_');
    if (parts.size() >= 2) {
        // 倒数第二个是 fps
        QString fps_str = parts[parts.size() - 2];
        bool ok;
        double fps = fps_str.toDouble(&ok);
        if (ok && fps > 0) {
            return fps;
        }
    }
    
    return 30.0;
}

uint32_t MainWindow::parse_frames_from_meta(const QString &video_path)
{
    // 从视频路径找到对应的 meta 文件并解析帧数
    QString meta_dir = QFileInfo(video_path).path() + "/.meta";
    QString video_basename = QFileInfo(video_path).completeBaseName();
    
    QDir dir(meta_dir);
    QStringList filters;
    filters << video_basename + "_*.jpeg";
    QStringList meta_files = dir.entryList(filters, QDir::Files);
    
    if (meta_files.isEmpty()) {
        return 0;
    }
    
    // 解析 frames: {basename}_{fps}_{frames}.jpeg
    QString meta_name = QFileInfo(meta_files.first()).completeBaseName();
    QStringList parts = meta_name.split('_');
    if (parts.size() >= 1) {
        // 最后一个是 frames
        QString frames_str = parts.last();
        bool ok;
        uint32_t frames = frames_str.toUInt(&ok);
        if (ok) {
            return frames;
        }
    }
    
    return 0;
}

void MainWindow::start_video_playback(const QString &video_path, const QStringList &video_list, int index)
{
    LOG_INFO("Starting video playback: {}", video_path.toStdString());
    
    // 停止相机（如果运行中）
    stop_camera();
    
    // 保存视频列表
    m_video_list = video_list;
    m_current_video_index = index;
    
    // 设置 FileOverlayPage
    ui->fileOverlayPage->set_video_list(video_list, index);
    ui->fileOverlayPage->set_filename(QFileInfo(video_path).fileName());
    ui->fileOverlayPage->set_time(0.0);
    
    // 解析 meta 信息
    double fps = parse_fps_from_meta(video_path);
    uint32_t total_frames = parse_frames_from_meta(video_path);
    
    LOG_INFO("Video meta: fps={:.2f}, frames={}", fps, total_frames);
    
    // 创建并启动 FilePlayer 线程
    if (m_file_player_thread) {
        m_file_player_thread->stop_player();
        m_file_player_thread->deleteLater();
    }
    
    m_file_player_thread = new PlayerThread(this);
    
    // 连接信号
    connect(m_file_player_thread, &PlayerThread::playback_ended,
            this, &MainWindow::stop_video_playback);
    
    // 设置 meta 信息和初始暂停
    m_file_player_thread->set_meta_info(fps, total_frames);
    m_file_player_thread->set_start_paused(true);  // 显示首帧后暂停
    
    // 配置 FilePlayer
    player::PlayerConfig config;
    config.camera_device = video_path.toStdString();  // 文件路径
    config.drm_device = "/dev/dri/card0";
    config.width = 1280;
    config.height = 720;
    config.fps = static_cast<uint32_t>(fps);
    config.zpos = 0;
    config.plane_id = 0;
    
    m_file_player_thread->start_player(config);
    m_is_playing = false;  // 初始暂停状态
    
    // 启动时间更新定时器
    m_playback_timer->start(100);
    
    // 显示 FileOverlayPage
    show_file_overlay_page();
}

void MainWindow::stop_video_playback()
{
    LOG_INFO("Stopping video playback");
    
    m_playback_timer->stop();
    m_is_playing = false;
    
    if (m_file_player_thread) {
        m_file_player_thread->stop_player();
        m_file_player_thread->deleteLater();
        m_file_player_thread = nullptr;
    }
    
    // 返回 videos 页面
    show_videos_page();
}

void MainWindow::toggle_video_play_pause()
{
    if (!m_file_player_thread) return;
    
    if (m_is_playing) {
        m_file_player_thread->pause_playback();
        m_is_playing = false;
        LOG_INFO("Video playback paused");
    } else {
        m_file_player_thread->resume_playback();
        m_is_playing = true;
        LOG_INFO("Video playback resumed");
    }
}

void MainWindow::play_previous_video()
{
    if (m_current_video_index > 0) {
        m_current_video_index--;
        QString video_path = m_video_list[m_current_video_index];
        start_video_playback(video_path, m_video_list, m_current_video_index);
    }
}

void MainWindow::play_next_video()
{
    if (m_current_video_index < m_video_list.size() - 1) {
        m_current_video_index++;
        QString video_path = m_video_list[m_current_video_index];
        start_video_playback(video_path, m_video_list, m_current_video_index);
    }
}

void MainWindow::delete_current_video(const QString &video_path)
{
    LOG_INFO("Delete video requested: {}", video_path.toStdString());
    
    // 停止播放
    if (m_file_player_thread) {
        m_file_player_thread->stop_player();
    }
    
    // 删除视频文件
    if (QFile::remove(video_path)) {
        LOG_INFO("Video deleted: {}", video_path.toStdString());
        
        // 从列表中移除
        m_video_list.removeAt(m_current_video_index);
        
        if (m_video_list.isEmpty()) {
            // 没有更多视频，返回列表
            stop_video_playback();
        } else {
            // 播放下一个（或上一个如果是最后一个）
            if (m_current_video_index >= m_video_list.size()) {
                m_current_video_index = m_video_list.size() - 1;
            }
            QString next_video = m_video_list[m_current_video_index];
            start_video_playback(next_video, m_video_list, m_current_video_index);
        }
    } else {
        LOG_ERROR("Failed to delete video: {}", video_path.toStdString());
    }
}

void MainWindow::update_playback_time()
{
    if (m_file_player_thread && m_file_player_thread->is_capturing()) {
        double time = m_file_player_thread->get_current_time();
        ui->fileOverlayPage->set_time(time);
    }
}

void MainWindow::start_camera()
{
    if (m_player_thread->isRunning()) {
        return;
    }
    
    player::PlayerConfig config;
    config.camera_device = "/dev/video51";
    config.drm_device = "/dev/dri/card0";
    config.width = 1280;
    config.height = 720;
    config.fps = 30;
    config.zpos = 0;
    config.plane_id = 0;
    
    LOG_INFO("Starting camera...");
    m_player_thread->start_player(config);
    
    // 启动自动重连定时器
    if (!m_camera_check_timer->isActive()) {
        connect(m_camera_check_timer, &QTimer::timeout,
                this, &MainWindow::check_camera_status);
        m_camera_check_timer->start(1000);
        LOG_DEBUG("Camera auto-reconnect timer started");
    }
}

void MainWindow::stop_camera()
{
    LOG_INFO("Stopping camera...");
    
    // 停止自动重连定时器
    if (m_camera_check_timer->isActive()) {
        m_camera_check_timer->stop();
        LOG_DEBUG("Camera auto-reconnect timer stopped");
    }
    
    m_player_thread->stop_player();
}

void MainWindow::load_style_sheet()
{
    QString combinedStyle;
    
    // 尝试从资源或本地目录加载所有样式表
    QStringList styleDirs = {
        STYLE_DIR,
    };
    
    for (const QString &dirPath : styleDirs) {
        QDir styleDir(dirPath);
        if (styleDir.exists()) {
            QStringList qssFiles = styleDir.entryList({"*.qss"}, QDir::Files);
            for (const QString &fileName : qssFiles) {
                QFile styleFile(styleDir.filePath(fileName));
                if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
                    combinedStyle += QString::fromUtf8(styleFile.readAll());
                    combinedStyle += "\n";
                    styleFile.close();
                }
            }
            if (!qssFiles.isEmpty()) {
                break;  // 找到样式文件后停止搜索
            }
        }
    }
    
    if (!combinedStyle.isEmpty()) {
        setStyleSheet(combinedStyle);
        LOG_INFO("All stylesheets loaded successfully");
    } else {
        LOG_WARN("Could not load any stylesheets");
    }
}

void MainWindow::load_translation()
{
    // 获取系统语言
    QString locale = QLocale::system().name();  // 例如 "zh_CN", "en_US"
    LOG_INFO("System locale: {}", locale.toStdString());
    
    // 尝试加载对应语言的翻译文件
    QString qmFile = TRANSLATIONS_DIR + locale + ".qm";
    
    if (!m_translator->load(qmFile)) {
        // 如果失败，尝试只用语言代码（如 zh）
        QString langCode = locale.split('_').first();
        qmFile = TRANSLATIONS_DIR + langCode + ".qm";
        
        if (!m_translator->load(qmFile)) {
            // 默认加载英文
            qmFile = TRANSLATIONS_DIR + "en.qm";
            m_translator->load(qmFile);
        }
    }
    
    QCoreApplication::installTranslator(m_translator);
    LOG_INFO("Loaded translation: {}", qmFile.toStdString());
}
