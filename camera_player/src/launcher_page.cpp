#include "launcher_page.hpp"
#include "ui_launcher_page.h"
#include "player/logger.hpp"
#include <QDateTime>
#include <QPixmap>
#include <QTransform>
#include <QPalette>
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>

static const QString BATTERY_ICON_PATH[] = {
    ":/resources/icon/battery_0.png",
    ":/resources/icon/battery_1.png",
    ":/resources/icon/battery_2.png",
    ":/resources/icon/battery_3.png",
    ":/resources/icon/battery_4.png"
};

static const QString LAUNCHER_ICON_PATH[] = {
    ":/resources/launcher/gallery.png",
    ":/resources/launcher/videos.png",
    ":/resources/launcher/settings.png",
    ":/resources/launcher/back.png"
};

LauncherPage::LauncherPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LauncherPage)
    , m_date_time_timer(new QTimer(this))
{
    ui->setupUi(this);
    
    load_icons();
    
    // 设置定时器更新时间
    update_date_time();
    connect(m_date_time_timer, &QTimer::timeout, this, &LauncherPage::update_date_time);
    m_date_time_timer->start(1000);
    
    // 为图标安装事件过滤器以支持点击
    ui->backIcon->installEventFilter(this);
    ui->galleryIcon->installEventFilter(this);
    ui->videosIcon->installEventFilter(this);
    ui->settingsIcon->installEventFilter(this);
    
    // 设置鼠标光标为手型
    ui->backIcon->setCursor(Qt::PointingHandCursor);
    ui->galleryIcon->setCursor(Qt::PointingHandCursor);
    ui->videosIcon->setCursor(Qt::PointingHandCursor);
    ui->settingsIcon->setCursor(Qt::PointingHandCursor);
}

LauncherPage::~LauncherPage()
{
    delete ui;
}

void LauncherPage::load_icons()
{
    // 加载电池图标 - 顺时针旋转90度
    QPixmap batteryPixmap(BATTERY_ICON_PATH[3]);
    if (!batteryPixmap.isNull()) {
        QTransform transform;
        transform.rotate(90);
        QPixmap rotatedBattery = batteryPixmap.transformed(transform, Qt::SmoothTransformation);
        ui->launcherBatteryIcon->setPixmap(rotatedBattery.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    // 加载启动器图标
    QPixmap galleryPixmap(LAUNCHER_ICON_PATH[0]);
    if (!galleryPixmap.isNull()) {
        ui->galleryIcon->setPixmap(galleryPixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    QPixmap videosPixmap(LAUNCHER_ICON_PATH[1]);
    if (!videosPixmap.isNull()) {
        ui->videosIcon->setPixmap(videosPixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    QPixmap settingsPixmap(LAUNCHER_ICON_PATH[2]);
    if (!settingsPixmap.isNull()) {
        ui->settingsIcon->setPixmap(settingsPixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    QPixmap backPixmap(LAUNCHER_ICON_PATH[3]);
    if (!backPixmap.isNull()) {
        ui->backIcon->setPixmap(backPixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void LauncherPage::update_date_time()
{
    QString dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    ui->launcherDateTimeLabel->setText(dateTime);
}

void LauncherPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
}

bool LauncherPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (obj == ui->backIcon) {
            LOG_INFO("Back icon clicked");
            emit back_clicked();
            return true;
        } else if (obj == ui->galleryIcon) {
            LOG_INFO("Gallery icon clicked");
            emit gallery_clicked();
            return true;
        } else if (obj == ui->videosIcon) {
            LOG_INFO("Videos icon clicked");
            emit videos_clicked();
            return true;
        } else if (obj == ui->settingsIcon) {
            LOG_INFO("Settings icon clicked");
            emit settings_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
