// 滚动数字选择器 - 参考 ipcamera rollingboxy
#include "rolling_box_y.hpp"

#include <QMouseEvent>
#include <QPainter>

RollingBoxY::RollingBoxY(QWidget *parent)
    : QWidget(parent)
    , m_minRange(0)
    , m_maxRange(100)
    , m_currentValue(0)
    , m_isDragging(false)
    , m_deviation(0)
    , m_mouseSrcPos(0)
    , m_numSize(5)
    , m_hasChoosed(false)
{
    setWindowFlags(windowFlags() & ~Qt::WindowMinMaxButtonsHint);
    setStyleSheet("background:white;");
    m_homingAni = new QPropertyAnimation(this, "deviation");
    m_homingAni->setDuration(300);
    m_homingAni->setEasingCurve(QEasingCurve::OutQuad);
}

RollingBoxY::RollingBoxY(QWidget *parent, int min, int max, int initialValue)
    : QWidget(parent)
    , m_minRange(min)
    , m_maxRange(max)
    , m_currentValue(initialValue)
    , m_isDragging(false)
    , m_deviation(0)
    , m_mouseSrcPos(0)
    , m_numSize(5)
    , m_hasChoosed(false)
{
    setWindowFlags(windowFlags() & ~Qt::WindowMinMaxButtonsHint);
    setStyleSheet("background:white;");
    m_homingAni = new QPropertyAnimation(this, "deviation");
    m_homingAni->setDuration(300);
    m_homingAni->setEasingCurve(QEasingCurve::OutQuad);
}

void RollingBoxY::setRange(int min, int max)
{
    m_minRange = min;
    m_maxRange = max;
    if (m_currentValue < min)
        m_currentValue = min;
    if (m_currentValue > max)
        m_currentValue = max;

    m_numSize = 3;
    int temp = m_maxRange;
    while (temp > 0) {
        temp /= 10;
        m_numSize++;
    }
    repaint();
}

void RollingBoxY::changeValue(int variate)
{
    m_currentValue += variate;
    if (m_currentValue < m_minRange)
        m_currentValue = m_minRange;
    if (m_currentValue > m_maxRange)
        m_currentValue = m_maxRange;
    repaint();
}

int RollingBoxY::readValue() { return m_currentValue; }

void RollingBoxY::choose(bool flag)
{
    m_hasChoosed = flag;
    repaint();
}

// 鼠标按下
void RollingBoxY::mousePressEvent(QMouseEvent *e)
{
    m_homingAni->stop();
    m_isDragging = true;
    m_mouseSrcPos = e->pos().y();
}

// 鼠标移动
void RollingBoxY::mouseMoveEvent(QMouseEvent *e)
{
    if (m_isDragging) {
        if ((m_currentValue == m_minRange && e->pos().y() >= m_mouseSrcPos) ||
            (m_currentValue == m_maxRange && e->pos().y() <= m_mouseSrcPos))
            return;

        m_deviation = e->pos().y() - m_mouseSrcPos;

        if (m_deviation > (height() - 1) / 4)
            m_deviation = (height() - 1) / 4;
        else if (m_deviation < -(height() - 1) / 4)
            m_deviation = -(height() - 1) / 4;

        emit deviationChange((float)m_deviation / ((height() - 1) / 5));
        repaint();
    }
}

// 鼠标松开
void RollingBoxY::mouseReleaseEvent(QMouseEvent *)
{
    if (m_isDragging) {
        m_isDragging = false;
        homing();
    }
}

// 绘图
void RollingBoxY::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int Width = width() - 1;
    int Height = height() - 1;

    // 偏移大于 1/4 时切换数值
    if (m_deviation >= Height / 4 && m_currentValue > m_minRange) {
        m_mouseSrcPos += Height / 4;
        m_deviation -= Height / 4;
        m_currentValue -= 1;
    }
    if (m_deviation <= -Height / 4 && m_currentValue < m_maxRange) {
        m_mouseSrcPos -= Height / 4;
        m_deviation += Height / 4;
        m_currentValue += 1;
    }

    // 中间数字
    paintNum(painter, m_currentValue, m_deviation);

    // 两侧数字
    if (m_currentValue != m_minRange)
        paintNum(painter, m_currentValue - 1, m_deviation - Height / 4);
    if (m_currentValue != m_maxRange)
        paintNum(painter, m_currentValue + 1, m_deviation + Height / 4);

    // 两侧数字2
    if (m_deviation >= 0 && m_currentValue - 2 >= m_minRange)
        paintNum(painter, m_currentValue - 2, m_deviation - Height / 2);
    if (m_deviation <= 0 && m_currentValue + 2 <= m_maxRange)
        paintNum(painter, m_currentValue + 2, m_deviation + Height / 2);

    // 中间选框线
    painter.setPen(QPen(QColor(70, 144, 249), 2));
    painter.drawLine(0, Height / 8 * 3, Width, Height / 8 * 3);
    painter.drawLine(0, Height / 8 * 5, Width, Height / 8 * 5);
}

// 画数字
void RollingBoxY::paintNum(QPainter &painter, int num, int deviation)
{
    int Width = width() - 1;
    int Height = height() - 1;

    int size = (Height - qAbs(deviation)) / m_numSize - 4;
    int transparency = 255 - 510 * qAbs(deviation) / Height;
    int h = Height / 2 - 3 * qAbs(deviation) / 5;
    int y = Height / 2 + deviation - h / 2;

    QFont font;
    font.setPixelSize(size);
    painter.setFont(font);

    if (!m_hasChoosed)
        painter.setPen(QColor(0, 0, 0, 120));
    else
        painter.setPen(QColor(0, 0, 0, transparency));

    painter.drawText(
        QRectF(0, y, Width, h),
        Qt::AlignCenter,
        QString::number(num)
    );
}

// 回弹到中心
void RollingBoxY::homing()
{
    if (m_deviation > height() / 8) {
        m_homingAni->setStartValue((height() - 1) / 8 - m_deviation);
        m_homingAni->setEndValue(0);
        m_currentValue--;
    } else if (m_deviation > -height() / 8) {
        m_homingAni->setStartValue(m_deviation);
        m_homingAni->setEndValue(0);
    } else if (m_deviation < -height() / 8) {
        m_homingAni->setStartValue(-(height() - 1) / 8 - m_deviation);
        m_homingAni->setEndValue(0);
        m_currentValue++;
    }
    emit currentValueChanged(m_currentValue);
    m_homingAni->start();
}

int RollingBoxY::readDeviation() { return m_deviation; }

void RollingBoxY::setDeviation(int n)
{
    m_deviation = n;
    repaint();
}
