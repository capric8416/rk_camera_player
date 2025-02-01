#include "gallery_page.hpp"
#include "player/logger.hpp"
#include "ui_gallery_page.h"

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

// 自定义可点击标签，用于全屏预览
class ClickableLabel : public QLabel
{
public:
    std::function<void(QPoint)> onPressed;
    std::function<void(QPoint)> onReleased;

    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (onPressed) {
            LOG_INFO("ClickableLabel: mousePressEvent");
            onPressed(event->globalPos());
        }
        QLabel::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (onReleased) {
            LOG_INFO("ClickableLabel: mouseReleaseEvent");
            onReleased(event->globalPos());
        }
        QLabel::mouseReleaseEvent(event);
    }
};

GalleryPage::GalleryPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GalleryPage)
    , m_fullscreen_label(nullptr)
    , m_fullscreen_name_label(nullptr)
    , m_fullscreen_visible(false)
    , m_current_image_index(-1)
    , m_images_dir("images")
    , m_current_row(0)
    , m_load_timer(new QTimer(this))
    , m_long_press_timer(new QTimer(this))
{
    ui->setupUi(this);

    // 动态设置列宽均匀分布 (每列 1/3)
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

    // 设置标题翻译
    ui->titleLabel->setText(tr("Gallery"));

    // 全屏预览标签 (初始隐藏) - 使用自定义 ClickableLabel
    auto *clickableLabel = new ClickableLabel(this);
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

        // 如果长按定时器已触发弹窗（时间被 invalidate），不再处理
        if (!m_press_time.isValid() ||
            m_press_time.elapsed() >= LONG_PRESS_MS) {
            LOG_INFO("Fullscreen: Long press already triggered or invalid");
            return;
        }

        int dy = releasePos.y() - m_press_pos.y();
        int dx = releasePos.x() - m_press_pos.x();
        int distance = (releasePos - m_press_pos).manhattanLength();

        LOG_INFO("Fullscreen: dx={}, dy={}, distance={}", dx, dy, distance);

        // 1. 检测上下滑动
        if (qAbs(dy) > SWIPE_THRESHOLD && qAbs(dy) > qAbs(dx)) {
            if (dy < 0) {
                // 向上滑动 → 返回列表
                LOG_INFO("Fullscreen: Swipe up - back to gallery");
                hide_fullscreen();
            }
            return;
        }

        // 2. 检测左右滑动 → 切换图片
        if (qAbs(dx) > SWIPE_THRESHOLD && qAbs(dx) > qAbs(dy)) {
            if (dx < 0) {
                LOG_INFO("Fullscreen: Swipe left - next image");
                show_next_image();
            } else {
                LOG_INFO("Fullscreen: Swipe right - previous image");
                show_previous_image();
            }
            return;
        }

        // 3. 短按无操作（避免误触）
        LOG_INFO("Fullscreen: Tap ignored (use swipe or long press)");
    };

    m_fullscreen_label = clickableLabel;

    // 全屏文件名标签 (右上角绿色)
    m_fullscreen_name_label = new QLabel(this);
    m_fullscreen_name_label->setAlignment(Qt::AlignRight | Qt::AlignTop);
    m_fullscreen_name_label->setStyleSheet(
        "color: #00ff00; font-size: 16px; background-color: transparent; "
        "padding: 8px;"
    );
    m_fullscreen_name_label->hide();

    // 操作菜单 (内嵌式，不使用 QMessageBox)
    // 操作菜单 (内嵌式，不使用 QMessageBox)
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

    // 删除确认菜单 (内嵌式)
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
        // 返回操作菜单
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
        delete_current_image();
    });
    confirmBtnLayout->addWidget(yesBtn);

    confirmLayout->addLayout(confirmBtnLayout);

    // 延迟加载定时器
    connect(m_load_timer, &QTimer::timeout, this, &GalleryPage::load_next_row);

    // 长按定时器
    m_long_press_timer->setSingleShot(true);
    connect(
        m_long_press_timer,
        &QTimer::timeout,
        this,
        &GalleryPage::handle_fullscreen_long_press
    );
}

GalleryPage::~GalleryPage()
{
    m_load_timer->stop();
    m_long_press_timer->stop();
    delete ui;
}

void GalleryPage::set_images_directory(const QString &path)
{
    m_images_dir = path;
}

void GalleryPage::refresh()
{
    // 停止正在进行的加载
    m_load_timer->stop();

    // 清除现有缩略图
    clear_grid();

    // 扫描图片文件
    scan_images();

    // 清除已加载文件列表
    m_loaded_files.clear();

    // 开始延迟加载
    m_current_row = 0;
    if (!m_pending_files.isEmpty()) {
        m_load_timer->start(LOAD_DELAY_MS);
    }
}

void GalleryPage::clear_grid()
{
    QLayoutItem *item;
    while ((item = ui->gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void GalleryPage::scan_images()
{
    m_pending_files.clear();

    QDir dir(m_images_dir);
    if (!dir.exists()) {
        LOG_WARN(
            "Images directory does not exist: {}",
            m_images_dir.toStdString()
        );
        return;
    }

    QStringList filters;
    filters << "*.jpeg"
            << "*.jpg"
            << "*.png"
            << "*.bmp";
    m_pending_files =
        dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

    LOG_INFO(
        "Found {} images in {}",
        m_pending_files.size(),
        m_images_dir.toStdString()
    );
}

void GalleryPage::load_next_row()
{
    // 每次加载一行 (COLUMNS 个图片)
    int startIdx = m_current_row * COLUMNS;
    int endIdx = qMin(startIdx + COLUMNS, m_pending_files.size());

    if (startIdx >= m_pending_files.size()) {
        m_load_timer->stop();
        LOG_INFO("All images loaded");
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

void GalleryPage::create_thumbnail(const QString &filepath, int row, int col)
{
    // 创建白色卡片容器 - 包含图片和文件名
    QWidget *container = new QWidget();
    container->setStyleSheet("background-color: white; border-radius: 4px;");
    container->setProperty("filepath", filepath);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    // 缩略图
    QLabel *thumbnail = new QLabel();
    thumbnail->setAlignment(Qt::AlignCenter);
    thumbnail->setStyleSheet("background-color: transparent;");
    thumbnail->setCursor(Qt::PointingHandCursor);
    thumbnail->setProperty("filepath", filepath);
    thumbnail->installEventFilter(this);

    QPixmap pixmap(filepath);
    if (!pixmap.isNull()) {
        thumbnail->setPixmap(pixmap.scaled(
            THUMBNAIL_SIZE,
            THUMBNAIL_SIZE,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));
    }

    layout->addWidget(thumbnail);

    // 文件名 - 黑色文字
    QFileInfo fileInfo(filepath);
    QLabel *nameLabel = new QLabel(fileInfo.fileName());
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet(
        "color: black; font-size: 14px; background-color: transparent;"
    );
    nameLabel->setWordWrap(true);
    layout->addWidget(nameLabel);

    ui->gridLayout->addWidget(container, row, col);
}

void GalleryPage::show_fullscreen(const QString &filepath)
{
    // 查找文件索引
    m_current_image_index = m_loaded_files.indexOf(filepath);
    if (m_current_image_index < 0) {
        m_current_image_index = 0;
    }

    show_fullscreen_by_index(m_current_image_index);
}

void GalleryPage::show_fullscreen_by_index(int index)
{
    if (index < 0 || index >= m_loaded_files.size()) {
        return;
    }

    QString filepath = m_loaded_files[index];
    m_current_image_index = index;

    QPixmap pixmap(filepath);
    if (pixmap.isNull()) {
        LOG_WARN("Failed to load image: {}", filepath.toStdString());
        return;
    }

    m_fullscreen_label->setGeometry(rect());
    m_fullscreen_label->setPixmap(
        pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    m_fullscreen_label->setProperty("filepath", filepath);
    m_fullscreen_label->show();
    m_fullscreen_label->raise();

    // 显示文件名
    QFileInfo fileInfo(filepath);
    m_fullscreen_name_label->setText(fileInfo.fileName());
    m_fullscreen_name_label->setGeometry(0, 0, width(), 40);
    m_fullscreen_name_label->show();
    m_fullscreen_name_label->raise();

    m_fullscreen_visible = true;
}

void GalleryPage::show_previous_image()
{
    if (m_current_image_index > 0) {
        show_fullscreen_by_index(m_current_image_index - 1);
    }
}

void GalleryPage::show_next_image()
{
    if (m_current_image_index < m_loaded_files.size() - 1) {
        show_fullscreen_by_index(m_current_image_index + 1);
    }
}

void GalleryPage::hide_fullscreen()
{
    m_fullscreen_label->hide();
    m_fullscreen_name_label->hide();

    // 隐藏所有弹窗
    if (m_action_menu)
        m_action_menu->hide();
    if (m_confirm_menu)
        m_confirm_menu->hide();

    m_fullscreen_visible = false;
    m_long_press_timer->stop();
}

void GalleryPage::show_action_menu()
{
    // 显示内嵌操作菜单
    LOG_INFO("Showing action menu overlay");

    // 居中显示菜单
    int x = (width() - m_action_menu->width()) / 2;
    int y = (height() - m_action_menu->height()) / 2;
    m_action_menu->move(x, y);
    m_action_menu->show();
    m_action_menu->raise();
}

void GalleryPage::hide_action_menu()
{
    LOG_INFO("Hiding action menu overlay");
    m_action_menu->hide();
}

void GalleryPage::delete_current_image()
{
    if (m_current_image_index < 0 ||
        m_current_image_index >= m_loaded_files.size()) {
        return;
    }

    QString filepath = m_loaded_files[m_current_image_index];
    QFileInfo fileInfo(filepath);

    // 二次确认已通过菜单完成，直接删除

    // 删除文件
    QFile file(filepath);
    if (file.remove()) {
        LOG_INFO("Deleted file: {}", filepath.toStdString());

        // 从列表移除
        m_loaded_files.removeAt(m_current_image_index);

        // 隐藏全屏
        hide_fullscreen();

        // 从 Grid 中查找并移除对应的 widget
        QWidget *targetWidget = nullptr;
        for (int i = 0; i < ui->gridLayout->count(); ++i) {
            QLayoutItem *item = ui->gridLayout->itemAt(i);
            if (item && item->widget()) {
                QString itemPath =
                    item->widget()->property("filepath").toString();
                if (itemPath == filepath) {
                    targetWidget = item->widget();
                    break;
                }
            }
        }

        if (targetWidget) {
            ui->gridLayout->removeWidget(targetWidget);
            delete targetWidget;

            // 重新排列剩余的 widgets
            rearrange_grid();
        } else {
            // 兜底：如果没找到 widget，只能刷新
            refresh();
        }
    } else {
        LOG_ERROR("Failed to delete file: {}", filepath.toStdString());
        // 使用简单的提示或忽略错误
        LOG_ERROR("Failed to delete file dialog not shown to avoid freeze");
    }
}

void GalleryPage::rearrange_grid()
{
    // 获取所有现有的 widgets
    QList<QWidget *> widgets;
    while (QLayoutItem *item = ui->gridLayout->takeAt(0)) {
        if (item->widget()) {
            widgets.append(item->widget());
        } else {
            delete item;
        }
    }

    // 重新按顺序添加到 grid
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

    // 更新当前行数以便后续加载
    m_current_row = row + (col > 0 ? 1 : 0);
}

void GalleryPage::resizeEvent(QResizeEvent *event)
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

bool GalleryPage::eventFilter(QObject *obj, QEvent *event)
{
    // 记录按下位置 (非全屏)
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        m_press_pos = mouseEvent->globalPos();
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        QPoint releasePos = mouseEvent->globalPos();
        int distance = (releasePos - m_press_pos).manhattanLength();

        // 返回按钮始终响应
        if (obj == ui->backLabel) {
            handle_back_clicked();
            return true;
        } else if (distance < TAP_THRESHOLD) {
            // 只有短按（移动距离小）才触发全屏
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

void GalleryPage::handle_back_clicked()
{
    if (m_fullscreen_visible) {
        hide_fullscreen();
    } else {
        emit back_clicked();
    }
}

void GalleryPage::handle_thumbnail_clicked()
{
    // Handled in eventFilter
}

void GalleryPage::handle_fullscreen_clicked() { hide_fullscreen(); }

void GalleryPage::handle_fullscreen_long_press()
{
    LOG_INFO("on_fullscreen_long_press triggered");
    m_long_press_timer->stop();

    // 重置按压时间，防止 release 事件误判为长按
    m_press_time.invalidate();

    show_action_menu();

    LOG_INFO("on_fullscreen_long_press finished");
}
