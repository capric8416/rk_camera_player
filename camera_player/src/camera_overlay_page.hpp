#ifndef CAMERA_OVERLAY_PAGE_H
#define CAMERA_OVERLAY_PAGE_H

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

namespace Ui {
class CameraOverlayPage;
}

/**
 * 捕获模式
 */
enum class CaptureMode {
    CameraNormal,  // 相机模式 (默认)
    VideoNormal,   // 视频模式 (待机)
    VideoRecording // 录像中
};

class CameraOverlayPage : public QWidget
{
    Q_OBJECT

public:
    explicit CameraOverlayPage(QWidget *parent = nullptr);
    ~CameraOverlayPage();

    void set_file_path(const QString &path);

    /**
     * 获取当前捕获模式
     */
    CaptureMode capture_mode() const { return m_capture_mode; }

    /**
     * 设置录像状态 (外部调用以更新图标)
     */
    void set_recording(bool recording);

signals:
    /**
     * 请求截图
     */
    void capture_image_requested();

    /**
     * 请求开始录像
     */
    void start_video_requested();

    /**
     * 请求停止录像
     */
    void stop_video_requested();

public slots:
    void update_date_time();
    void update_record_timer();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void handle_long_press_timeout();
    void handle_capture_feedback_timeout();

private:
    void load_icons();
    void update_icon();
    void handle_short_press();
    void handle_long_press();

    Ui::CameraOverlayPage *ui;
    QTimer *m_date_time_timer;
    QTimer *m_record_timer;
    QTimer *m_long_press_timer;
    QTimer *m_capture_feedback_timer;
    QElapsedTimer m_press_elapsed;
    QElapsedTimer m_record_elapsed;

    CaptureMode m_capture_mode;
    bool m_long_press_triggered;

    static constexpr int LONG_PRESS_THRESHOLD_MS = 2500;
    static constexpr int CAPTURE_FEEDBACK_MS = 1000;
};

#endif // CAMERA_OVERLAY_PAGE_H
