#ifndef VIDEOS_PAGE_H
#define VIDEOS_PAGE_H

#include <QElapsedTimer>
#include <QFileInfoList>
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace Ui {
class VideosPage;
}

/**
 * VideosPage - 视频列表页面
 * 显示视频元数据缩略图，支持全屏预览（目前显示首帧）
 */
class VideosPage : public QWidget
{
    Q_OBJECT

public:
    explicit VideosPage(QWidget *parent = nullptr);
    ~VideosPage();

    /**
     * 设置视频目录路径
     */
    void set_videos_directory(const QString &path);

    /**
     * 刷新列表
     */
    void refresh();

signals:
    /**
     * 返回按钮点击信号
     */
    void back_clicked();

    /**
     * 视频选中信号 (传递 meta 文件路径、视频文件路径、视频列表、当前索引)
     */
    void video_selected(
        const QString &video_path,
        const QStringList &video_list,
        int index
    );

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void handle_back_clicked();
    void handle_thumbnail_clicked();
    void handle_fullscreen_clicked();
    void load_next_row();
    void handle_fullscreen_long_press();

private:
    void scan_videos();
    void create_thumbnail(const QString &meta_filepath, int row, int col);
    void show_fullscreen(const QString &meta_filepath);
    void show_fullscreen_by_index(int index);
    void hide_fullscreen();
    void clear_grid();
    void rearrange_grid();
    void show_previous_video();
    void show_next_video();
    void show_action_menu();
    void hide_action_menu();
    void delete_current_video();

    // 辅助：从元数据文件名解析出视频文件名
    QString get_video_filename(const QString &meta_filename);
    QString get_video_filepath(const QString &meta_filepath);

    Ui::VideosPage *ui;

    // 全屏预览
    QLabel *m_fullscreen_label;
    QLabel *m_fullscreen_name_label;
    bool m_fullscreen_visible;
    int m_current_index;

    // 操作菜单
    QWidget *m_action_menu;
    QPushButton *m_return_btn;
    QPushButton *m_delete_btn;

    // 删除确认菜单
    QWidget *m_confirm_menu;

    // 数据
    QString m_videos_dir;
    QString m_meta_dir;
    QFileInfoList m_pending_files; // 待加载的 meta 文件列表
    QList<QString> m_loaded_files; // 已加载的 meta 文件路径列表
    int m_current_row;

    // 延迟加载定时器
    QTimer *m_load_timer;

    // 触摸检测
    QPoint m_press_pos;
    QElapsedTimer m_press_time;
    QTimer *m_long_press_timer;

    static constexpr int COLUMNS = 3;
    static constexpr int THUMBNAIL_SIZE = 400;
    static constexpr int LOAD_DELAY_MS = 16;
    static constexpr int TAP_THRESHOLD = 20;
    static constexpr int SWIPE_THRESHOLD = 100;
    static constexpr int LONG_PRESS_MS = 1000;
};

#endif // VIDEOS_PAGE_H
