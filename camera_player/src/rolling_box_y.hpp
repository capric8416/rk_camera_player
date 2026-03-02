#ifndef ROLLING_BOX_Y_HPP
#define ROLLING_BOX_Y_HPP

#include <QPropertyAnimation>
#include <QWidget>

/**
 * RollingBoxY - 滚动数字选择器
 *
 * 自定义绘制的垂直滚动数字选择控件。
 * 参考 ipcamera 的 rollingboxy。
 * 支持触摸拖拽和按键操作。
 */
class RollingBoxY : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int deviation READ readDeviation WRITE setDeviation)

public:
    explicit RollingBoxY(QWidget *parent = nullptr);
    explicit RollingBoxY(QWidget *parent, int min, int max, int initialValue);

    void setRange(int min, int max);
    void changeValue(int variate);
    void choose(bool flag);
    int readValue();

signals:
    void currentValueChanged(int value);
    void deviationChange(float deviation);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    void paintNum(QPainter &painter, int num, int deviation);
    void homing();
    int readDeviation();
    void setDeviation(int n);

    int m_minRange;
    int m_maxRange;
    int m_currentValue;
    bool m_isDragging;
    int m_deviation;
    int m_mouseSrcPos;
    int m_numSize;
    bool m_hasChoosed;
    QPropertyAnimation *m_homingAni;
};

#endif // ROLLING_BOX_Y_HPP
