#include "languages_page.hpp"
#include "player/logger.hpp"
#include "ui_languages_page.h"

#include <QEvent>
#include <QFile>
#include <QMouseEvent>

LanguagesPage::LanguagesPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LanguagesPage)
{
    ui->setupUi(this);

    // 加载样式表
    load_stylesheet();

    // 设置列表项
    setup_list_items();

    // 事件过滤器
    ui->backLabel->setCursor(Qt::PointingHandCursor);
    ui->backLabel->installEventFilter(this);
    ui->titleLabel->setCursor(Qt::PointingHandCursor);
    ui->titleLabel->installEventFilter(this);

    // 列表点击事件
    connect(
        ui->languageList,
        &QListWidget::itemClicked,
        this,
        &LanguagesPage::handle_item_clicked
    );
}

LanguagesPage::~LanguagesPage() { delete ui; }

void LanguagesPage::load_stylesheet()
{
    QFile styleFile(":/resources/style/languages_page.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = styleFile.readAll();
        setStyleSheet(styleSheet);
        styleFile.close();
    }
}

static const char *LANGUAGE_NAMES[] = {
    QT_TR_NOOP("English"),
    QT_TR_NOOP("简体中文"),
    QT_TR_NOOP("日本"),
    QT_TR_NOOP("한국어"),
    QT_TR_NOOP("Español"),
    QT_TR_NOOP("Deutsch"),
    QT_TR_NOOP("Українська"),
    QT_TR_NOOP("بالعربية"),
    QT_TR_NOOP("Português"),
    QT_TR_NOOP("Français"),
    QT_TR_NOOP("Русский"),
    QT_TR_NOOP("ไทย"),
    QT_TR_NOOP("Türkçe"),
    QT_TR_NOOP("Tiếng Việt"),
    QT_TR_NOOP("Polski")
};

void LanguagesPage::setup_list_items()
{
    // 设置列表显示模式
    ui->languageList->setViewMode(QListWidget::IconMode);
    ui->languageList->setResizeMode(QListWidget::Adjust);
    ui->languageList->setSpacing(5);
    // 禁用拖拽
    ui->languageList->setDragDropMode(QAbstractItemView::NoDragDrop);

    ui->languageList->clear();

    // 计算每个item的宽度 (总宽 - margin/padding) / 3
    // 假设列表占据大部分宽度，这里取固定值或者动态计算
    // 在构造函数中可能拿不到准确宽度，可以用 resizeEvent 或先给个近似值
    // 参考 ipcamera logic: int s=m_lanList->width()-40; int oneWidth=s/3;
    // 这里先硬编码一个合适的值，或者后续在 resizeEvent 中调整
    int itemWidth = 253; // 800 width / 3 approx
    int itemHeight = 65;

    for (const char *lang : LANGUAGE_NAMES) {
        QListWidgetItem *item = new QListWidgetItem(tr(lang));
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(itemWidth, itemHeight));
        ui->languageList->addItem(item);
    }
}

void LanguagesPage::update_selected_language(int index)
{
    if (index >= 0 && index < ui->languageList->count()) {
        ui->languageList->setCurrentRow(index);
    }
}

void LanguagesPage::handle_item_clicked(QListWidgetItem *item)
{
    int index = ui->languageList->row(item);
    LOG_INFO("Language selected index: {}", index);

    emit language_selected(index);
}

bool LanguagesPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (obj == ui->backLabel || obj == ui->titleLabel) {
            emit back_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LanguagesPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 动态调整 item 大小
    if (ui->languageList->count() > 0) {
        int listWidth = ui->languageList->width();
        // 减去 padding 和 spacing
        int itemWidth = (listWidth - 40) / 3;
        int itemHeight = 65;

        for (int i = 0; i < ui->languageList->count(); ++i) {
            ui->languageList->item(i)->setSizeHint(QSize(itemWidth, itemHeight)
            );
        }
    }
}

void LanguagesPage::translateUi()
{
    ui->titleLabel->setText(tr("Language"));

    // 更新列表项文字
    for (int i = 0;
         i < ui->languageList->count() &&
         i < (int)(sizeof(LANGUAGE_NAMES) / sizeof(LANGUAGE_NAMES[0]));
         ++i) {
        ui->languageList->item(i)->setText(tr(LANGUAGE_NAMES[i]));
    }
}
