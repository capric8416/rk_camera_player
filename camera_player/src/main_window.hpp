#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include "player_thread.hpp"

namespace Ui {
class MainWindow;
}

class StdinCommandHandler;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void show_camera_overlay_page();
    void show_file_overlay_page();
    void show_launcher_page();
    void show_gallery_page();
    void show_videos_page();
    void show_settings_page();
    void show_languages_page();

public slots:
    void change_language(int index);
    void start_camera();
    void stop_camera();
    void capture_image();
    void start_video_capture();
    void stop_video_capture();

private slots:
    void handle_camera_connected();
    void handle_camera_disconnected();
    void handle_back_clicked();
    void check_camera_status();

    // 视频播放相关
    void start_video_playback(
        const QString &video_path,
        const QStringList &video_list,
        int index
    );
    void stop_video_playback();
    void toggle_video_play_pause();
    void play_previous_video();
    void play_next_video();
    void delete_current_video(const QString &video_path);
    void update_playback_time();

private:
    void load_style_sheet();
    void load_translation();
    double parse_fps_from_meta(const QString &video_path);
    uint32_t parse_frames_from_meta(const QString &video_path);

    Ui::MainWindow *ui;
    QTranslator *m_translator;
    QTimer *m_camera_check_timer;
    QTimer *m_playback_timer;
    PlayerThread *m_player_thread;
    PlayerThread *m_file_player_thread;
    StdinCommandHandler *m_stdin_handler;

    // 视频播放状态
    QStringList m_video_list;
    int m_current_video_index;
    bool m_is_playing;
};

#endif // MAIN_WINDOW_H
