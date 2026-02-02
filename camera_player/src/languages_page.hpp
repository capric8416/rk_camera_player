#ifndef LANGUAGES_PAGE_HPP
#define LANGUAGES_PAGE_HPP

#include <QListWidget>
#include <QWidget>

namespace Ui {
class LanguagesPage;
}

/**
 * LanguagesPage - 语言选择页面
 */
class LanguagesPage : public QWidget
{
    Q_OBJECT

public:
    explicit LanguagesPage(QWidget *parent = nullptr);
    ~LanguagesPage();

    /**
     * 更新选中的语言
     * @param index 0: English, 1: Chinese
     */
    void update_selected_language(int index);

    /**
     * 更新界面翻译
     */
    void translateUi();

signals:
    /**
     * 返回按钮点击信号
     */
    void back_clicked();

    /**
     * 语言选择信号
     * @param index 0: English, 1: Chinese
     */
    void language_selected(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setup_list_items();
    void load_stylesheet();
    void handle_item_clicked(QListWidgetItem *item);

    Ui::LanguagesPage *ui;
};

#endif // LANGUAGES_PAGE_HPP
