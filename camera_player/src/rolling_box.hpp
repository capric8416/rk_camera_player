#ifndef ROLLING_BOX_HPP
#define ROLLING_BOX_HPP

#include <QTimer>
#include <QWidget>

class RollingBoxY;

/**
 * RollingBox - 日期时间滚动选择器
 *
 * 组合 6 个 RollingBoxY（年/月/日/时/分/秒），
 * 参考 ipcamera 的 rollingbox。
 */
class RollingBox : public QWidget
{
    Q_OBJECT

public:
    explicit RollingBox(QWidget *parent = nullptr);

    int readYear();
    int readMonth();
    int readDay();
    int readHour();
    int readMinute();
    int readSecond();

    /**
     * 选中某一列 (0=年, 1=月, 2=日, 3=时, 4=分, 5=秒, -1=无)
     */
    void choose(int index);

    /**
     * 当前选中列的值 +/- variate
     */
    void changeTimebox(int variate);

public slots:
    void receiveValue();

private:
    QTimer *m_timer;
    RollingBoxY *m_rby;  // 年
    RollingBoxY *m_rbm;  // 月
    RollingBoxY *m_rbd;  // 日
    RollingBoxY *m_rbh;  // 时
    RollingBoxY *m_rbmi; // 分
    RollingBoxY *m_rbs;  // 秒
    int m_pos;
    int m_maxRange;

    int maxDayValue();
};

#endif // ROLLING_BOX_HPP
