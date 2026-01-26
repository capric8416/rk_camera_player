#include "camera_overlay_page.hpp"
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QPixmap>
#include "player/logger.hpp"
#include "ui_camera_overlay_page.h"

// 图标路径
static const QString ICON_CAMERA_NORMAL = ":/resources/icon/camera_normal.png";
static const QString ICON_CAMERA_CAPTURE =
    ":/resources/icon/camera_capture.png";
static const QString ICON_VIDEO_NORMAL = ":/resources/icon/video_normal.png";
static const QString ICON_VIDEO_RECORDING =
    ":/resources/icon/video_recording.png";

static const QString SD_CARD_ICON_30_PERCENT_FREE =
    ":/resources/icon/sd_card_0.png";
static const QString SD_CARD_ICON_60_PERCENT_FREE =
    ":/resources/icon/sd_card_1.png";
static const QString SD_CARD_ICON_90_PERCENT_FREE =
    ":/resources/icon/sd_card_2.png";

static const QString BATTERY_ICON_20_PERCENT_FREE =
    ":/resources/icon/battery_0.png";
static const QString BATTERY_ICON_40_PERCENT_FREE =
    ":/resources/icon/battery_1.png";
static const QString BATTERY_ICON_60_PERCENT_FREE =
    ":/resources/icon/battery_2.png";
static const QString BATTERY_ICON_80_PERCENT_FREE =
    ":/resources/icon/battery_3.png";
static const QString BATTERY_ICON_100_PERCENT_FREE =
    ":/resources/icon/battery_4.png";

CameraOverlayPage::CameraOverlayPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CameraOverlayPage)
    , m_date_time_timer(new QTimer(this))
    , m_record_timer(new QTimer(this))
    , m_long_press_timer(new QTimer(this))
    , m_capture_feedback_timer(new QTimer(this))
    , m_capture_mode(CaptureMode::CameraNormal)
    , m_long_press_triggered(false)
{
    ui->setupUi(this);

    load_icons();

    // 设置事件过滤器处理图标点击
    ui->cameraIcon->installEventFilter(this);
    ui->cameraIcon->setCursor(Qt::PointingHandCursor);

    // 长按定时器 (单次触发)
    m_long_press_timer->setSingleShot(true);
    connect(
        m_long_press_timer,
        &QTimer::timeout,
        this,
        &CameraOverlayPage::handle_long_press_timeout
    );

    // 截图反馈定时器 (单次触发)
    m_capture_feedback_timer->setSingleShot(true);
    connect(
        m_capture_feedback_timer,
        &QTimer::timeout,
        this,
        &CameraOverlayPage::handle_capture_feedback_timeout
    );

    // 设置定时器更新时间
    update_date_time();
    connect(
        m_date_time_timer,
        &QTimer::timeout,
        this,
        &CameraOverlayPage::update_date_time
    );
    m_date_time_timer->start(1000);

    // 录像计时器
    connect(
        m_record_timer,
        &QTimer::timeout,
        this,
        &CameraOverlayPage::update_record_timer
    );
}

void CameraOverlayPage::update_record_timer()
{
    qint64 elapsed = m_record_elapsed.elapsed() / 1000;
    int h = elapsed / 3600;
    int m = (elapsed % 3600) / 60;
    int s = elapsed % 60;

    QString timeStr = QString("%1:%2:%3")
                          .arg(h, 2, 10, QChar('0'))
                          .arg(m, 2, 10, QChar('0'))
                          .arg(s, 2, 10, QChar('0'));

    ui->filePathLabel->setText(timeStr);
}

CameraOverlayPage::~CameraOverlayPage() { delete ui; }

void CameraOverlayPage::load_icons()
{
    // 默认显示相机图标
    update_icon();

    // 加载SD卡图标
    QPixmap sdCardPixmap(SD_CARD_ICON_30_PERCENT_FREE);
    if (!sdCardPixmap.isNull()) {
        ui->sdCardIcon->setPixmap(
            sdCardPixmap
                .scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
    }

    // 加载电池图标
    QPixmap batteryPixmap(BATTERY_ICON_80_PERCENT_FREE);
    if (!batteryPixmap.isNull()) {
        ui->batteryIcon->setPixmap(
            batteryPixmap
                .scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
    }
}

void CameraOverlayPage::update_icon()
{
    QString iconPath;

    switch (m_capture_mode) {
    case CaptureMode::CameraNormal:
        iconPath = ICON_CAMERA_NORMAL;
        ui->filePathLabel->setText(tr("image mode"));
        ui->filePathLabel->setStyleSheet("color: #00ff00;");
        break;
    case CaptureMode::VideoNormal:
        iconPath = ICON_VIDEO_NORMAL;
        ui->filePathLabel->setText(tr("video mode"));
        ui->filePathLabel->setStyleSheet("color: #00ff00;");
        break;
    case CaptureMode::VideoRecording:
        iconPath = ICON_VIDEO_RECORDING;
        // 录像路径在 set_file_path 中设置
        ui->filePathLabel->setStyleSheet("color: #ffff00;");
        break;
    }

    QPixmap pixmap(iconPath);
    if (!pixmap.isNull()) {
        ui->cameraIcon->setPixmap(
            pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
    }
}

void CameraOverlayPage::set_file_path(const QString &path)
{
    ui->filePathLabel->setText(path);
    ui->filePathLabel->setStyleSheet("color: #ffff00;");
}

void CameraOverlayPage::set_recording(bool recording)
{
    if (recording) {
        m_capture_mode = CaptureMode::VideoRecording;
    } else {
        m_capture_mode = CaptureMode::VideoNormal;
    }
    update_icon();
}

void CameraOverlayPage::update_date_time()
{
    QString dateTime =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    ui->dateTimeLabel->setText(dateTime);
}

void CameraOverlayPage::handle_long_press_timeout()
{
    m_long_press_triggered = true;
    handle_long_press();
}

void CameraOverlayPage::handle_capture_feedback_timeout()
{
    // 截图反馈结束，恢复正常图标
    if (m_capture_mode == CaptureMode::CameraNormal) {
        update_icon();
    }
}

void CameraOverlayPage::handle_short_press()
{
    switch (m_capture_mode) {
    case CaptureMode::CameraNormal:
        // 短按截图
        LOG_INFO("Camera mode: capture image");
        // 显示截图图标
        {
            QPixmap pixmap(ICON_CAMERA_CAPTURE);
            if (!pixmap.isNull()) {
                ui->cameraIcon->setPixmap(pixmap.scaled(
                    32,
                    32,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                ));
            }
        }
        m_capture_feedback_timer->start(CAPTURE_FEEDBACK_MS);
        emit capture_image_requested();
        break;

    case CaptureMode::VideoNormal:
        // 短按开始录像
        LOG_INFO("Video mode: start recording");
        m_capture_mode = CaptureMode::VideoRecording;
        update_icon();

        // 开始计时
        m_record_elapsed.start();
        update_record_timer();
        m_record_timer->start(1000);

        emit start_video_requested();
        break;

    case CaptureMode::VideoRecording:
        // 短按停止录像
        LOG_INFO("Recording mode: stop recording");
        m_capture_mode = CaptureMode::VideoNormal;

        // 停止计时
        m_record_timer->stop();

        update_icon();
        emit stop_video_requested();
        break;
    }
}

void CameraOverlayPage::handle_long_press()
{
    switch (m_capture_mode) {
    case CaptureMode::CameraNormal:
        // 长按切换到视频模式
        LOG_INFO("Switch to video mode");
        m_capture_mode = CaptureMode::VideoNormal;
        update_icon();
        break;

    case CaptureMode::VideoNormal:
        // 长按切换回相机模式
        LOG_INFO("Switch to camera mode");
        m_capture_mode = CaptureMode::CameraNormal;
        update_icon();
        break;

    case CaptureMode::VideoRecording:
        // 长按停止录像
        LOG_INFO("Recording mode: stop recording (long press)");
        m_capture_mode = CaptureMode::CameraNormal;

        // 停止计时
        m_record_timer->stop();

        update_icon();
        emit stop_video_requested();
        break;
    }
}

bool CameraOverlayPage::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->cameraIcon) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_long_press_triggered = false;
                m_press_elapsed.start();
                m_long_press_timer->start(LONG_PRESS_THRESHOLD_MS);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_long_press_timer->stop();
                if (!m_long_press_triggered) {
                    // 短按
                    handle_short_press();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
