#include "settings_page.hpp"
#include "player/logger.hpp"
#include "ui_settings_page.h"

#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPixmap>

// 图标路径
const QString SettingsPage::ICON_PATHS[] = {
    ":/resources/icon/language_set.png",
    ":/resources/icon/date_time_set.png",
    ":/resources/icon/brightness_set.png",
    ":/resources/icon/back_set.png"
};

const QString SettingsPage::ARROW_ICON_PATH =
    ":/resources/icon/right_arrow.png";

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
    , m_current_index(0)
{
    ui->setupUi(this);

    // 加载样式表
    load_stylesheet();

    // 初始化选项文字（支持多语言）
    m_option_texts.clear();
    m_option_texts.append(tr("Language Setting"));
    m_option_texts.append(tr("Change Date&Time"));
    m_option_texts.append(tr("Adjust Screen Brightness"));
    m_option_texts.append(tr("Back"));

    // 设置列表项
    setup_list_items();

    // 设置标题翻译
    ui->titleLabel->setText(tr("Settings"));

    // 设置返回箭头和标题事件过滤器
    ui->backLabel->setCursor(Qt::PointingHandCursor);
    ui->backLabel->installEventFilter(this);
    ui->titleLabel->setCursor(Qt::PointingHandCursor);
    ui->titleLabel->installEventFilter(this);

    // 设置列表间距
    ui->optionList->setSpacing(5);

    // 默认选中第一项
    set_item_selected(m_current_index, true);
}

SettingsPage::~SettingsPage() { delete ui; }

void SettingsPage::load_stylesheet()
{
    QFile styleFile(":/resources/style/settings_page.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = styleFile.readAll();
        setStyleSheet(styleSheet);
        styleFile.close();
    }
}

void SettingsPage::setup_list_items()
{
    for (int i = 0; i < ITEM_COUNT; ++i) {
        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(700, 80));

        QWidget *widget = create_item_widget(ICON_PATHS[i], i);
        m_item_widgets.append(widget);

        ui->optionList->addItem(item);
        ui->optionList->setItemWidget(item, widget);
    }
}

QWidget *
SettingsPage::create_item_widget(const QString &iconPath, int textIndex)
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("optionItemWidget");
    widget->setProperty("selected", false);
    widget->setAttribute(Qt::WA_Hover, true);
    widget->installEventFilter(this);
    widget->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(35, 0, 15, 0);

    // 左侧图标
    QLabel *iconLabel = new QLabel();
    iconLabel->setObjectName("iconLabel");
    QPixmap iconPixmap(iconPath);
    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(
            iconPixmap
                .scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
    }
    iconLabel->setFixedSize(50, 40);
    layout->addWidget(iconLabel);

    // 中间文字
    QLabel *textLabel = new QLabel();
    textLabel->setObjectName("textLabel");
    textLabel->setText(m_option_texts.at(textIndex));
    m_text_labels.append(textLabel);
    layout->addWidget(textLabel);

    // 添加拉伸以将箭头推到右边
    layout->addStretch();

    // 右侧箭头
    QLabel *arrowLabel = new QLabel();
    arrowLabel->setObjectName("arrowLabel");
    QPixmap arrowPixmap(ARROW_ICON_PATH);
    if (!arrowPixmap.isNull()) {
        arrowLabel->setPixmap(
            arrowPixmap
                .scaled(19, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
    }
    arrowLabel->setFixedSize(50, 40);
    layout->addWidget(arrowLabel);

    return widget;
}

void SettingsPage::set_item_selected(int index, bool selected)
{
    if (index < 0 || index >= m_item_widgets.size()) {
        return;
    }

    QWidget *widget = m_item_widgets.at(index);
    widget->setProperty("selected", selected);

    // 更新样式
    QString bgColor = selected ? "rgb(111,156,211)" : "white";
    widget->setStyleSheet(
        QString(
            "QWidget#optionItemWidget { background-color: %1; border: none; }"
        )
            .arg(bgColor)
    );

    // 更新子标签样式
    QList<QLabel *> labels = widget->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        label->setStyleSheet(
            QString("QLabel { background-color: %1; border: none; }")
                .arg(bgColor)
        );
    }
}

void SettingsPage::handle_item_clicked(int index)
{
    LOG_INFO("Settings item clicked: {}", index);

    // 更新选中状态
    if (m_current_index != index) {
        set_item_selected(m_current_index, false);
        m_current_index = index;
        set_item_selected(m_current_index, true);
    }

    // 发出对应信号
    switch (index) {
    case 0:
        emit language_setting_clicked();
        break;
    case 1:
        emit datetime_setting_clicked();
        break;
    case 2:
        emit brightness_setting_clicked();
        break;
    case 3:
        emit back_clicked();
        break;
    default:
        break;
    }
}

bool SettingsPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        // 标题栏返回
        if (obj == ui->backLabel || obj == ui->titleLabel) {
            LOG_INFO("Settings: back clicked from header");
            emit back_clicked();
            return true;
        }

        // 列表项点击
        for (int i = 0; i < m_item_widgets.size(); ++i) {
            if (obj == m_item_widgets.at(i)) {
                handle_item_clicked(i);
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SettingsPage::translateUi()
{
    // 更新选项文字
    QStringList newTexts;
    newTexts.append(tr("Language Setting"));
    newTexts.append(tr("Change Date&Time"));
    newTexts.append(tr("Adjust Screen Brightness"));
    newTexts.append(tr("Back"));

    for (int i = 0; i < m_text_labels.size() && i < newTexts.size(); ++i) {
        m_text_labels.at(i)->setText(newTexts.at(i));
    }

    // 更新标题
    ui->titleLabel->setText(tr("Settings"));
}
