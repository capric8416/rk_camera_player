#ifndef LAUNCHER_PAGE_H
#define LAUNCHER_PAGE_H

#include <QTimer>
#include <QWidget>

namespace Ui {
class LauncherPage;
}

class LauncherPage : public QWidget
{
    Q_OBJECT

public:
    explicit LauncherPage(QWidget *parent = nullptr);
    ~LauncherPage();

signals:
    void gallery_clicked();
    void videos_clicked();
    void settings_clicked();
    void back_clicked();

public slots:
    void update_date_time();
    void translateUi();

private:
    void load_icons();

    Ui::LauncherPage *ui;
    QTimer *m_date_time_timer;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // LAUNCHER_PAGE_H
