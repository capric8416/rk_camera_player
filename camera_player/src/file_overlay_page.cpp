#include "file_overlay_page.hpp"
#include "ui_file_overlay_page.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QVBoxLayout>

FileOverlayPage::FileOverlayPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FileOverlayPage)
    , m_current_index(-1)
    , m_long_press_timer(new QTimer(this))
    , m_long_press_triggered(false)
    , m_action_menu(nullptr)
    , m_confirm_menu(nullptr)
{
    ui->setupUi(this);

    // 设置透明背景
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background-color: transparent;");

    // 加载样式表
    QFile styleFile(":/resources/style/file_overlay_page.qss");
    if (styleFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // 安装事件过滤器
    installEventFilter(this);

    // 长按定时器
    m_long_press_timer->setSingleShot(true);
    connect(
        m_long_press_timer,
        &QTimer::timeout,
        this,
        &FileOverlayPage::handle_long_press_timeout
    );

    // 创建操作菜单
    create_action_menu();

    // 初始化显示
    set_time(0.0);
    set_filename("");
}

FileOverlayPage::~FileOverlayPage() { delete ui; }

void FileOverlayPage::set_time(const QString &time)
{
    ui->timeLabel->setText(time);
}

void FileOverlayPage::set_time(double seconds)
{
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int secs = total_seconds % 60;

    QString time_str = QString("%1:%2:%3")
                           .arg(hours, 2, 10, QChar('0'))
                           .arg(minutes, 2, 10, QChar('0'))
                           .arg(secs, 2, 10, QChar('0'));

    ui->timeLabel->setText(time_str);
}

void FileOverlayPage::set_filename(const QString &filename)
{
    ui->filenameLabel->setText(filename);
}

void FileOverlayPage::set_video_list(
    const QStringList &video_files,
    int current_index
)
{
    m_video_files = video_files;
    m_current_index = current_index;

    if (m_current_index >= 0 && m_current_index < m_video_files.size()) {
        set_filename(QFileInfo(m_video_files[m_current_index]).fileName());
    }
}

QString FileOverlayPage::current_video_path() const
{
    if (m_current_index >= 0 && m_current_index < m_video_files.size()) {
        return m_video_files[m_current_index];
    }
    return QString();
}

void FileOverlayPage::create_action_menu()
{
    // 创建操作菜单 (居中显示)
    m_action_menu = new QWidget(this);
    m_action_menu->setObjectName("actionMenu");
    m_action_menu->setStyleSheet(
        "QWidget#actionMenu { background-color: rgba(0, 0, 0, 180); "
        "border-radius: 10px; }"
        "QPushButton { background-color: #333; color: white; border: none; "
        "padding: 15px "
        "30px; font-size: 18px; border-radius: 5px; }"
        "QPushButton:pressed { background-color: #555; }"
    );

    auto *layout = new QVBoxLayout(m_action_menu);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    m_back_btn = new QPushButton(tr("Return"), m_action_menu);
    m_prev_btn = new QPushButton(tr("Previous"), m_action_menu);
    m_next_btn = new QPushButton(tr("Next"), m_action_menu);
    m_delete_btn = new QPushButton(tr("Delete"), m_action_menu);
    m_delete_btn->setStyleSheet(
        "QPushButton { background-color: #a00; } QPushButton:pressed { "
        "background-color: #c00; }"
    );

    layout->addWidget(m_back_btn);
    layout->addWidget(m_prev_btn);
    layout->addWidget(m_next_btn);
    layout->addWidget(m_delete_btn);

    m_action_menu->setLayout(layout);
    m_action_menu->adjustSize();
    m_action_menu->hide();

    // 连接信号
    connect(m_back_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        emit back_clicked();
    });

    connect(m_prev_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        if (m_current_index > 0) {
            m_current_index--;
            emit previous_video_requested();
        }
    });

    connect(m_next_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        if (m_current_index < m_video_files.size() - 1) {
            m_current_index++;
            emit next_video_requested();
        }
    });

    connect(m_delete_btn, &QPushButton::clicked, this, [this]() {
        hide_action_menu();
        QString path = current_video_path();
        if (!path.isEmpty()) {
            emit delete_video_requested(path);
        }
    });

    // 创建删除确认菜单
    m_confirm_menu = new QWidget(this);
    m_confirm_menu->setObjectName("confirmMenu");
    m_confirm_menu->setStyleSheet(
        "QWidget#confirmMenu { background-color: rgba(0, 0, 0, 200); "
        "border-radius: 10px; }"
        "QPushButton { background-color: #333; color: white; border: none; "
        "padding: 15px "
        "30px; font-size: 18px; border-radius: 5px; }"
        "QPushButton:pressed { background-color: #555; }"
        "QLabel { color: white; font-size: 16px; }"
    );

    auto *confirmLayout = new QVBoxLayout(m_confirm_menu);
    m_confirm_label = new QLabel(tr("Confirm Delete?"), m_confirm_menu);
    m_confirm_label->setAlignment(Qt::AlignCenter);

    auto *btnLayout = new QHBoxLayout();
    m_cancel_btn = new QPushButton(tr("Cancel"), m_confirm_menu);
    m_confirm_btn = new QPushButton(tr("Confirm"), m_confirm_menu);
    m_confirm_btn->setStyleSheet(
        "QPushButton { background-color: #a00; } QPushButton:pressed { "
        "background-color: #c00; }"
    );

    btnLayout->addWidget(m_cancel_btn);
    btnLayout->addWidget(m_confirm_btn);

    confirmLayout->addWidget(m_confirm_label);
    confirmLayout->addLayout(btnLayout);
    m_confirm_menu->setLayout(confirmLayout);
    m_confirm_menu->adjustSize();
    m_confirm_menu->hide();

    connect(m_cancel_btn, &QPushButton::clicked, this, [this]() {
        m_confirm_menu->hide();
    });
}

void FileOverlayPage::show_action_menu()
{
    // 更新按钮状态
    m_prev_btn->setEnabled(m_current_index > 0);
    m_next_btn->setEnabled(m_current_index < m_video_files.size() - 1);

    // 居中显示
    m_action_menu->move(
        (width() - m_action_menu->width()) / 2,
        (height() - m_action_menu->height()) / 2
    );
    m_action_menu->show();
    m_action_menu->raise();
}

void FileOverlayPage::hide_action_menu() { m_action_menu->hide(); }

void FileOverlayPage::handle_long_press_timeout()
{
    m_long_press_triggered = true;
    show_action_menu();
}

bool FileOverlayPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        m_press_pos = me->pos();
        m_press_elapsed.start();
        m_long_press_triggered = false;
        m_long_press_timer->start(LONG_PRESS_MS);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        m_long_press_timer->stop();
        auto *me = static_cast<QMouseEvent *>(event);

        if (m_long_press_triggered) {
            // 长按已处理
            return true;
        }

        QPoint delta = me->pos() - m_press_pos;
        int elapsed = m_press_elapsed.elapsed();

        // 检查滑动
        if (qAbs(delta.y()) > SWIPE_THRESHOLD &&
            qAbs(delta.y()) > qAbs(delta.x())) {
            if (delta.y() < 0) {
                // 上滑 -> 下一个
                if (m_current_index < m_video_files.size() - 1) {
                    m_current_index++;
                    emit next_video_requested();
                }
            } else {
                // 下滑 -> 上一个
                if (m_current_index > 0) {
                    m_current_index--;
                    emit previous_video_requested();
                }
            }
            return true;
        }

        // 检查双击
        if (elapsed < 200 && m_last_click_time.isValid() &&
            m_last_click_time.elapsed() < DOUBLE_CLICK_MS) {
            emit toggle_play_pause();
            m_last_click_time.invalidate();
            return true;
        }

        m_last_click_time.start();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void FileOverlayPage::translateUi()
{
    m_back_btn->setText(tr("Return"));
    m_prev_btn->setText(tr("Previous"));
    m_next_btn->setText(tr("Next"));
    m_delete_btn->setText(tr("Delete"));
    m_confirm_label->setText(tr("Confirm Delete?"));
    m_cancel_btn->setText(tr("Cancel"));
    m_confirm_btn->setText(tr("Confirm"));
}
