#include "rolling_box.hpp"
#include "rolling_box_y.hpp"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>

RollingBox::RollingBox(QWidget *parent) : QWidget(parent), m_pos(-1)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(1100, 240);

    QHBoxLayout *layout = new QHBoxLayout();
    setLayout(layout);

    QDateTime now = QDateTime::currentDateTime();

    // 年
    m_rby = new RollingBoxY(this, 2000, 2099, now.date().year());
    m_rby->setFixedSize(90, 140);
    layout->addWidget(m_rby);

    QLabel *label = new QLabel(tr("Year"));
    label->setFixedSize(50, 140);
    layout->addWidget(label);

    // 月
    m_rbm = new RollingBoxY(this, 1, 12, now.date().month());
    m_rbm->setFixedSize(80, 140);
    layout->addWidget(m_rbm);

    label = new QLabel(tr("Month"));
    label->setFixedSize(50, 140);
    layout->addWidget(label);

    // 日
    m_rbd = new RollingBoxY(this, 1, 31, now.date().day());
    m_rbd->setFixedSize(80, 140);
    layout->addWidget(m_rbd);

    label = new QLabel(tr("Day"));
    label->setFixedSize(50, 140);
    layout->addWidget(label);

    // 间隔
    layout->addSpacing(30);

    // 时
    m_rbh = new RollingBoxY(this, 0, 23, now.time().hour());
    m_rbh->setFixedSize(80, 140);
    layout->addWidget(m_rbh);

    label = new QLabel(":");
    label->setFixedSize(20, 140);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    // 分
    m_rbmi = new RollingBoxY(this, 0, 59, now.time().minute());
    m_rbmi->setFixedSize(80, 140);
    layout->addWidget(m_rbmi);

    label = new QLabel(":");
    label->setFixedSize(20, 140);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    // 秒
    m_rbs = new RollingBoxY(this, 0, 59, now.time().second());
    m_rbs->setFixedSize(80, 140);
    layout->addWidget(m_rbs);

    // 定时器：每帧更新日期最大值
    m_timer = new QTimer(this);
    m_timer->start(33);
    connect(m_timer, &QTimer::timeout, this, &RollingBox::receiveValue);
}

int RollingBox::maxDayValue()
{
    int y = m_rby->readValue();
    int m = m_rbm->readValue();
    if (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12)
        return 31;
    if (m == 4 || m == 6 || m == 9 || m == 11)
        return 30;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return 28;
}

void RollingBox::receiveValue() { m_rbd->setRange(1, maxDayValue()); }

void RollingBox::choose(int index)
{
    m_rby->choose(index == 0);
    m_rbm->choose(index == 1);
    m_rbd->choose(index == 2);
    m_rbh->choose(index == 3);
    m_rbmi->choose(index == 4);
    m_rbs->choose(index == 5);
    m_pos = index;
    changeTimebox(0);
}

void RollingBox::changeTimebox(int variate)
{
    switch (m_pos) {
    case 0:
        m_rby->changeValue(variate);
        break;
    case 1:
        m_rbm->changeValue(variate);
        break;
    case 2:
        m_rbd->changeValue(variate);
        break;
    case 3:
        m_rbh->changeValue(variate);
        break;
    case 4:
        m_rbmi->changeValue(variate);
        break;
    case 5:
        m_rbs->changeValue(variate);
        break;
    }
}

int RollingBox::readYear() { return m_rby->readValue(); }
int RollingBox::readMonth() { return m_rbm->readValue(); }
int RollingBox::readDay() { return m_rbd->readValue(); }
int RollingBox::readHour() { return m_rbh->readValue(); }
int RollingBox::readMinute() { return m_rbmi->readValue(); }
int RollingBox::readSecond() { return m_rbs->readValue(); }
