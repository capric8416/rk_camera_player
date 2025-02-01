#ifndef GALLERY_PAGE_H
#define GALLERY_PAGE_H

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
class GalleryPage;
}

/**
 * GalleryPage - 图片库页面
 * 显示截图列表，支持缩略图预览和全屏查看
 */
class GalleryPage : public QWidget
{
    Q_OBJECT

public:
    explicit GalleryPage(QWidget *parent = nullptr);
    ~GalleryPage();

    /**
     * 设置图片目录路径
     */
    void set_images_directory(const QString &path);

    /**
     * 刷新图片列表
     */
    void refresh();

    /**
     * 隐藏操作菜单
     */
    void hide_action_menu();

signals:
    /**
     * 返回按钮点击信号
     */
    void back_clicked();

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
    void scan_images();
    void create_thumbnail(const QString &filepath, int row, int col);
    void show_fullscreen(const QString &filepath);
    void show_fullscreen_by_index(int index);
    void hide_fullscreen();
    void clear_grid();
    void rearrange_grid();
    void show_previous_image();
    void show_next_image();
    void show_action_menu();
    void delete_current_image();

    Ui::GalleryPage *ui;

    // 全屏预览
    QLabel *m_fullscreen_label;
    QLabel *m_fullscreen_name_label;
    bool m_fullscreen_visible;
    int m_current_image_index;

    // 操作菜单
    QWidget *m_action_menu;
    QPushButton *m_return_btn;
    QPushButton *m_delete_btn;

    // 删除确认菜单
    QWidget *m_confirm_menu;

    // 数据
    QString m_images_dir;
    QFileInfoList m_pending_files;
    QList<QString> m_loaded_files;
    int m_current_row;

    // 延迟加载定时器
    QTimer *m_load_timer;

    // 触摸检测 - 区分滑动和点击
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

#endif // GALLERY_PAGE_H
