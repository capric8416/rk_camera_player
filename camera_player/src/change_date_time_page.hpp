#ifndef CHANGE_DATE_TIME_PAGE_HPP
#define CHANGE_DATE_TIME_PAGE_HPP

#include <QKeyEvent>
#include <QWidget>

class RollingBox;

namespace Ui {
class ChangeDateTimePage;
}

/**
 * ChangeDateTimePage - 日期时间设置页面
 *
 * 使用 RollingBox 滚动选择器修改系统日期和时间。
 * 参考 ipcamera 的 changedatebox。
 */
class ChangeDateTimePage : public QWidget
{
    Q_OBJECT

public:
    explicit ChangeDateTimePage(QWidget *parent = nullptr);
    ~ChangeDateTimePage();

    void refresh();
    void translateUi();

signals:
    void back_clicked();

private slots:
    void on_cancel_clicked();
    void on_confirm_clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    Ui::ChangeDateTimePage *ui;
    RollingBox *m_rolling_box;
    int m_btn_pos;
    int m_btn_num;
};

#endif // CHANGE_DATE_TIME_PAGE_HPP
