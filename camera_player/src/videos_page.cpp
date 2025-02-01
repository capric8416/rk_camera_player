#include "videos_page.hpp"
#include "player/logger.hpp"
#include "ui_videos_page.h"

#include <functional>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScroller>
#include <QVBoxLayout>

// 复用 ClickableLabel (也可以抽取到公共组件，这里先复制一份以保持独立)
class VideoClickableLabel : public QLabel
{
public:
    std::function<void(QPoint)> onPressed;
    std::function<void(QPoint)> onReleased;

    explicit VideoClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (onPressed) {
            onPressed(event->globalPos());
        }
        QLabel::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (onReleased) {
            onReleased(event->globalPos());
        }
        QLabel::mouseReleaseEvent(event);
    }
};

VideosPage::VideosPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideosPage)
    , m_fullscreen_label(nullptr)
    , m_fullscreen_name_label(nullptr)
    , m_fullscreen_visible(false)
    , m_current_index(-1)
    , m_videos_dir("videos")
    , m_meta_dir("videos/.meta")
    , m_current_row(0)
    , m_load_timer(new QTimer(this))
    , m_long_press_timer(new QTimer(this))
{
    ui->setupUi(this);

    // 动态设置列宽均匀分布
    for (int i = 0; i < COLUMNS; ++i) {
        ui->gridLayout->setColumnStretch(i, 1);
    }

    // 设置布局对齐方式为顶部对齐，防止行过高
    ui->gridLayout->setAlignment(Qt::AlignTop);

    // 启用触摸滚动
    QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::TouchGesture);

    // 设置返回箭头事件
    ui->backLabel->setCursor(Qt::PointingHandCursor);
    ui->backLabel->installEventFilter(this);

    // 设置标题
    ui->titleLabel->setText(tr("Videos"));

    // 全屏预览标签
    auto *clickableLabel = new VideoClickableLabel(this);
    clickableLabel->setAlignment(Qt::AlignCenter);
    clickableLabel->setStyleSheet("background-color: black;");
    clickableLabel->setCursor(Qt::PointingHandCursor);
    clickableLabel->hide();

    // 设置回调
    clickableLabel->onPressed = [this](QPoint pos) {
        m_press_pos = pos;
        m_press_time.start();
        m_long_press_timer->start(LONG_PRESS_MS);
    };

    clickableLabel->onReleased = [this](QPoint releasePos) {
        m_long_press_timer->stop();

        if (!m_press_time.isValid() ||
            m_press_time.elapsed() >= LONG_PRESS_MS) {
            return;
        }

        int dy = releasePos.y() - m_press_pos.y();
        int dx = releasePos.x() - m_press_pos.x();

        // 检查上下滑动
        if (qAbs(dy) > SWIPE_THRESHOLD && qAbs(dy) > qAbs(dx)) {
            if (dy < 0) {
                show_next_video();
            } else {
                show_previous_video();
            }
            return;
        }
    };

    m_fullscreen_label = clickableLabel;

    // 全屏文件名标签
    m_fullscreen_name_label = new QLabel(this);
    m_fullscreen_name_label->setAlignment(Qt::AlignRight | Qt::AlignTop);
    m_fullscreen_name_label->setStyleSheet(
        "color: #00ff00; font-size: 16px; background-color: transparent; "
        "padding: 8px;"
    );
    m_fullscreen_name_label->hide();

    // 操作菜单
    m_action_menu = new QWidget(this);
    m_action_menu->setStyleSheet(
        "background-color: rgba(0, 0, 0, 200); border-radius: 8px;"
    );
    m_action_menu->setFixedSize(320, 100);
    m_action_menu->hide();

    QHBoxLayout *menuLayout = new QHBoxLayout(m_action_menu);
    menuLayout->setContentsMargins(16, 16, 16, 16);
    menuLayout->setSpacing(20);

    m_return_btn = new QPushButton(tr("Return"), m_action_menu);
    m_return_btn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-size: "
        "18px; padding: 8px; border-radius: 4px; }"
    );
    m_return_btn->setCursor(Qt::PointingHandCursor);
    m_return_btn->setFixedHeight(60);
    connect(m_return_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        hide_fullscreen();
    });
    menuLayout->addWidget(m_return_btn);

    m_delete_btn = new QPushButton(tr("Delete"), m_action_menu);
    m_delete_btn->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; font-size: "
        "18px; padding: 8px; border-radius: 4px; }"
    );
    m_delete_btn->setCursor(Qt::PointingHandCursor);
    m_delete_btn->setFixedHeight(60);
    connect(m_delete_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        // 显示删除确认菜单
        m_confirm_menu->move(
            (width() - m_confirm_menu->width()) / 2,
            (height() - m_confirm_menu->height()) / 2
        );
        m_confirm_menu->show();
        m_confirm_menu->raise();
    });
    menuLayout->addWidget(m_delete_btn);

    // 删除确认菜单
    m_confirm_menu = new QWidget(this);
    m_confirm_menu->setStyleSheet(
        "background-color: rgba(0, 0, 0, 200); border-radius: 8px;"
    );
    m_confirm_menu->setFixedSize(320, 140);
    m_confirm_menu->hide();

    QVBoxLayout *confirmLayout = new QVBoxLayout(m_confirm_menu);
    confirmLayout->setContentsMargins(16, 16, 16, 16);
    confirmLayout->setSpacing(12);

    QLabel *confirmLabel = new QLabel(tr("Confirm Delete?"), m_confirm_menu);
    confirmLabel->setStyleSheet(
        "color: white; font-size: 16px; background-color: transparent;"
    );
    confirmLabel->setAlignment(Qt::AlignCenter);
    confirmLayout->addWidget(confirmLabel);

    QHBoxLayout *confirmBtnLayout = new QHBoxLayout();
    confirmBtnLayout->setSpacing(20);

    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), m_confirm_menu);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-size: "
        "16px; padding: 8px; border-radius: 4px; }"
    );
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setFixedHeight(48);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_confirm_menu->hide();
        show_action_menu();
    });
    confirmBtnLayout->addWidget(cancelBtn);

    QPushButton *yesBtn = new QPushButton(tr("Yes"), m_confirm_menu);
    yesBtn->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; font-size: "
        "16px; padding: 8px; border-radius: 4px; }"
    );
    yesBtn->setCursor(Qt::PointingHandCursor);
    yesBtn->setFixedHeight(48);
    connect(yesBtn, &QPushButton::clicked, this, [this]() {
        m_confirm_menu->hide();
        delete_current_video();
    });
    confirmBtnLayout->addWidget(yesBtn);

    confirmLayout->addLayout(confirmBtnLayout);

    // 延迟加载定时器
    connect(m_load_timer, &QTimer::timeout, this, &VideosPage::load_next_row);

    // 长按定时器
    m_long_press_timer->setSingleShot(true);
    connect(
        m_long_press_timer,
        &QTimer::timeout,
        this,
        &VideosPage::handle_fullscreen_long_press
    );
}

VideosPage::~VideosPage()
{
    m_load_timer->stop();
    m_long_press_timer->stop();
    delete ui;
}

void VideosPage::set_videos_directory(const QString &path)
{
    m_videos_dir = path;
    m_meta_dir = path + "/.meta";
}

void VideosPage::refresh()
{
    m_load_timer->stop();
    clear_grid();
    scan_videos();
    m_loaded_files.clear();
    m_current_row = 0;

    if (!m_pending_files.isEmpty()) {
        m_load_timer->start(LOAD_DELAY_MS);
    }
}

void VideosPage::clear_grid()
{
    QLayoutItem *item;
    while ((item = ui->gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void VideosPage::scan_videos()
{
    m_pending_files.clear();

    QDir dir(m_meta_dir);
    if (!dir.exists()) {
        LOG_WARN("Meta directory does not exist: {}", m_meta_dir.toStdString());
        return;
    }

    QStringList filters;
    filters << "*.jpeg"
            << "*.jpg";
    m_pending_files =
        dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

    LOG_INFO(
        "Found {} video metas in {}",
        m_pending_files.size(),
        m_meta_dir.toStdString()
    );
}

void VideosPage::load_next_row()
{
    int startIdx = m_current_row * COLUMNS;
    int endIdx = qMin(startIdx + COLUMNS, m_pending_files.size());

    if (startIdx >= m_pending_files.size()) {
        m_load_timer->stop();
        LOG_INFO("All videos loaded");
        return;
    }

    for (int i = startIdx; i < endIdx; ++i) {
        QString filepath = m_pending_files[i].absoluteFilePath();
        m_loaded_files.append(filepath);
        int col = i % COLUMNS;
        create_thumbnail(filepath, m_current_row, col);
    }

    m_current_row++;
}

QString VideosPage::get_video_filename(const QString &meta_filename)
{
    // meta: YYYY-MM-DD_HH-MM-SS-sss_fps_frames.jpeg
    // 假设最后一个下划线后是 "frames.jpeg"，倒数第二个是 "fps"
    // 其实我们只需要找到前三个部分: YYYY-MM-DD_HH-MM-SS-sss

    // 简单解析：找到从右边数第三个下划线，或者使用字符串分割
    // 更健壮的方式是按照我们在 Player::save_captured_image 定义的格式解析

    // 格式: {basename}_{fps}_{frames}.jpeg
    // basename 包含下划线，例如 2026-01-17_15-00-00_123

    // 从文件名去掉扩展名
    QString stem =
        QFileInfo(meta_filename).completeBaseName(); // 使用 completeBaseName
                                                     // 以保留 .fps 等带点的部分
    // stem 如: 2026-01-17_15-00-00_123_30.0fps_100frames

    // 从后往前找两个下划线
    int lastUS = stem.lastIndexOf('_'); // before frames
    if (lastUS == -1)
        return meta_filename;

    int secondLastUS = stem.lastIndexOf('_', lastUS - 1); // before fps
    if (secondLastUS == -1)
        return meta_filename;

    // 截取 basename
    QString basename = stem.left(secondLastUS);
    return basename + ".mp4";
}

QString VideosPage::get_video_filepath(const QString &meta_filepath)
{
    QString meta_filename = QFileInfo(meta_filepath).fileName();
    QString video_filename = get_video_filename(meta_filename);
    return m_videos_dir + "/" + video_filename;
}

void VideosPage::create_thumbnail(
    const QString &meta_filepath,
    int row,
    int col
)
{
    QWidget *container = new QWidget();
    container->setStyleSheet("background-color: white; border-radius: 4px;");
    container->setProperty("filepath", meta_filepath);

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    // 缩略图
    QLabel *thumbnail = new QLabel();
    thumbnail->setAlignment(Qt::AlignCenter);
    thumbnail->setStyleSheet("background-color: transparent;");
    thumbnail->setCursor(Qt::PointingHandCursor);
    thumbnail->setProperty("filepath", meta_filepath);
    thumbnail->installEventFilter(this);

    QPixmap pixmap(meta_filepath);
    if (!pixmap.isNull()) {
        thumbnail->setPixmap(pixmap.scaled(
            THUMBNAIL_SIZE,
            THUMBNAIL_SIZE,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));
    }

    layout->addWidget(thumbnail);

    // 显示视频文件名
    QString videoName = get_video_filename(QFileInfo(meta_filepath).fileName());
    QLabel *nameLabel = new QLabel(videoName);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet(
        "color: black; font-size: 14px; background-color: transparent;"
    );
    nameLabel->setWordWrap(true);
    layout->addWidget(nameLabel);

    ui->gridLayout->addWidget(container, row, col);
}

void VideosPage::show_fullscreen(const QString &meta_filepath)
{
    m_current_index = m_loaded_files.indexOf(meta_filepath);
    if (m_current_index < 0)
        m_current_index = 0;

    show_fullscreen_by_index(m_current_index);
}

void VideosPage::show_fullscreen_by_index(int index)
{
    if (index < 0 || index >= m_loaded_files.size())
        return;

    QString meta_filepath = m_loaded_files[index];
    m_current_index = index;

    // 构建视频路径列表
    QStringList video_paths;
    for (const QString &meta : m_loaded_files) {
        video_paths.append(get_video_filepath(meta));
    }

    QString video_path = get_video_filepath(meta_filepath);

    // 发出视频选中信号，由 MainWindow 启动播放器
    emit video_selected(video_path, video_paths, index);
}

void VideosPage::show_previous_video()
{
    if (m_current_index > 0) {
        show_fullscreen_by_index(m_current_index - 1);
    }
}

void VideosPage::show_next_video()
{
    if (m_current_index < m_loaded_files.size() - 1) {
        show_fullscreen_by_index(m_current_index + 1);
    }
}

void VideosPage::hide_fullscreen()
{
    m_fullscreen_label->hide();
    m_fullscreen_name_label->hide();

    if (m_action_menu)
        m_action_menu->hide();
    if (m_confirm_menu)
        m_confirm_menu->hide();

    m_fullscreen_visible = false;
    m_long_press_timer->stop();
}

void VideosPage::show_action_menu()
{
    int x = (width() - m_action_menu->width()) / 2;
    int y = (height() - m_action_menu->height()) / 2;
    m_action_menu->move(x, y);
    m_action_menu->show();
    m_action_menu->raise();
}

void VideosPage::hide_action_menu() { m_action_menu->hide(); }

void VideosPage::delete_current_video()
{
    if (m_current_index < 0 || m_current_index >= m_loaded_files.size())
        return;

    QString meta_filepath = m_loaded_files[m_current_index];
    QString video_filepath = get_video_filepath(meta_filepath);

    // 1. 删除视频文件
    QFile videoFile(video_filepath);
    if (videoFile.exists() && !videoFile.remove()) {
        LOG_ERROR("Failed to delete video: {}", video_filepath.toStdString());
    } else {
        LOG_INFO("Deleted video: {}", video_filepath.toStdString());
    }

    // 2. 删除元数据图片
    QFile metaFile(meta_filepath);
    if (metaFile.remove()) {
        LOG_INFO("Deleted meta file: {}", meta_filepath.toStdString());

        m_loaded_files.removeAt(m_current_index);
        hide_fullscreen();

        // 从 Grid 移除
        QWidget *targetWidget = nullptr;
        for (int i = 0; i < ui->gridLayout->count(); ++i) {
            QLayoutItem *item = ui->gridLayout->itemAt(i);
            if (item && item->widget()) {
                QString itemPath =
                    item->widget()->property("filepath").toString();
                if (itemPath == meta_filepath) {
                    targetWidget = item->widget();
                    break;
                }
            }
        }

        if (targetWidget) {
            ui->gridLayout->removeWidget(targetWidget);
            delete targetWidget;
            rearrange_grid();
        } else {
            refresh();
        }
    } else {
        LOG_ERROR(
            "Failed to delete meta file: {}",
            meta_filepath.toStdString()
        );
    }
}

void VideosPage::rearrange_grid()
{
    QList<QWidget *> widgets;
    while (QLayoutItem *item = ui->gridLayout->takeAt(0)) {
        if (item->widget()) {
            widgets.append(item->widget());
        } else {
            delete item;
        }
    }

    int row = 0;
    int col = 0;
    for (QWidget *widget : widgets) {
        ui->gridLayout->addWidget(widget, row, col);
        col++;
        if (col >= COLUMNS) {
            col = 0;
            row++;
        }
    }
    m_current_row = row + (col > 0 ? 1 : 0);
}

void VideosPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fullscreen_visible && m_fullscreen_label) {
        QString filepath = m_fullscreen_label->property("filepath").toString();
        if (!filepath.isEmpty()) {
            QPixmap pixmap(filepath);
            m_fullscreen_label->setGeometry(rect());
            m_fullscreen_label->setPixmap(pixmap.scaled(
                size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            ));
        }
    }
}

bool VideosPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        m_press_pos = mouseEvent->globalPos();
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        QPoint releasePos = mouseEvent->globalPos();
        int distance = (releasePos - m_press_pos).manhattanLength();

        if (obj == ui->backLabel) {
            handle_back_clicked();
            return true;
        } else if (distance < TAP_THRESHOLD) {
            QLabel *label = qobject_cast<QLabel *>(obj);
            if (label && label->property("filepath").isValid()) {
                QString filepath = label->property("filepath").toString();
                show_fullscreen(filepath);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VideosPage::handle_back_clicked()
{
    if (m_fullscreen_visible) {
        hide_fullscreen();
    } else {
        emit back_clicked();
    }
}

void VideosPage::handle_thumbnail_clicked() {}
void VideosPage::handle_fullscreen_clicked() { hide_fullscreen(); }
void VideosPage::handle_fullscreen_long_press()
{
    m_long_press_timer->stop();
    m_press_time.invalidate();
    show_action_menu();
}
