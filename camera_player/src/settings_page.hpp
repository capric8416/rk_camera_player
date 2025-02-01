#ifndef SETTINGS_PAGE_HPP
#define SETTINGS_PAGE_HPP

#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QWidget>

namespace Ui {
class SettingsPage;
}

/**
 * SettingsPage - 设置页面
 * 显示设置选项列表，支持多语言
 */
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage();

    /**
     * 更新界面语言
     */
    void translateUi();

signals:
    /**
     * 返回按钮点击信号
     */
    void back_clicked();

    /**
     * 语言设置点击信号
     */
    void language_setting_clicked();

    /**
     * 日期时间设置点击信号
     */
    void datetime_setting_clicked();

    /**
     * 屏幕亮度设置点击信号
     */
    void brightness_setting_clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setup_list_items();
    void load_stylesheet();
    QWidget *create_item_widget(const QString &iconPath, int textIndex);
    void set_item_selected(int index, bool selected);
    void handle_item_clicked(int index);

    Ui::SettingsPage *ui;

    // 列表项 widget 列表
    QList<QWidget *> m_item_widgets;

    // 列表项文字标签列表（用于翻译更新）
    QList<QLabel *> m_text_labels;

    // 当前选中项索引
    int m_current_index;

    // 设置项文字
    QStringList m_option_texts;

    // 图标路径
    static const QString ICON_PATHS[];
    static const QString ARROW_ICON_PATH;

    // 列表项数量
    static constexpr int ITEM_COUNT = 4;
};

#endif // SETTINGS_PAGE_HPP
