#ifndef FILE_OVERLAY_PAGE_H
#define FILE_OVERLAY_PAGE_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <QPushButton>

namespace Ui {
class FileOverlayPage;
}

/**
 * FileOverlayPage - 文件播放叠加页面
 * 
 * 显示播放时间和文件名
 * 支持双击播放/暂停，长按弹出菜单，上下滑动切换视频
 */
class FileOverlayPage : public QWidget
{
    Q_OBJECT

public:
    explicit FileOverlayPage(QWidget *parent = nullptr);
    ~FileOverlayPage();

    /**
     * 设置播放时间 (HH:MM:SS 格式)
     */
    void set_time(const QString &time);

    /**
     * 设置播放时间 (秒)
     */
    void set_time(double seconds);

    /**
     * 设置文件名
     */
    void set_filename(const QString &filename);

    /**
     * 设置视频列表和当前索引
     */
    void set_video_list(const QStringList &video_files, int current_index);

    /**
     * 获取当前视频路径
     */
    QString current_video_path() const;

    /**
     * 获取当前索引
     */
    int current_index() const { return m_current_index; }

signals:
    /**
     * 返回信号
     */
    void back_clicked();

    /**
     * 双击播放/暂停切换
     */
    void toggle_play_pause();

    /**
     * 请求切换到上一个视频
     */
    void previous_video_requested();

    /**
     * 请求切换到下一个视频
     */
    void next_video_requested();

    /**
     * 请求删除当前视频
     */
    void delete_video_requested(const QString &video_path);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void handle_long_press_timeout();

private:
    void show_action_menu();
    void hide_action_menu();
    void create_action_menu();

    Ui::FileOverlayPage *ui;

    // 视频列表
    QStringList m_video_files;
    int m_current_index;

    // 手势检测
    QPoint m_press_pos;
    QElapsedTimer m_press_elapsed;
    QTimer *m_long_press_timer;
    QElapsedTimer m_last_click_time;
    bool m_long_press_triggered;

    // 操作菜单
    QWidget *m_action_menu;
    QPushButton *m_back_btn;
    QPushButton *m_prev_btn;
    QPushButton *m_next_btn;
    QPushButton *m_delete_btn;

    // 删除确认菜单
    QWidget *m_confirm_menu;

    static constexpr int DOUBLE_CLICK_MS = 300;
    static constexpr int LONG_PRESS_MS = 1000;
    static constexpr int SWIPE_THRESHOLD = 100;
};

#endif // FILE_OVERLAY_PAGE_H
