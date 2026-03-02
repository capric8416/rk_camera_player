#include "change_date_time_page.hpp"
#include "player/logger.hpp"
#include "rolling_box.hpp"
#include "ui_change_date_time_page.h"

#include <QProcess>
#include <QSettings>

static const QString SETTINGS_KEY_TIME_FORMAT = "settings/time_format";

ChangeDateTimePage::ChangeDateTimePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChangeDateTimePage)
    , m_rolling_box(nullptr)
    , m_btn_pos(0)
    , m_btn_num(8) // 6 columns + cancel + confirm
{
    ui->setupUi(this);
    installEventFilter(this);

    // 标题
    ui->titleLabel->setText(tr("Change Date&Time"));
    ui->titleLabel->setStyleSheet(
        "QLabel{background-color:rgb(111,156,211);border:none;"
        "font-size:35px;color:white}"
    );

    // 按钮
    ui->cancelButton->setText(tr("Cancel"));
    ui->confirmButton->setText(tr("Confirm"));
    ui->cancelButton->setStyleSheet(
        "QPushButton{background-color:white;text-align:center;"
        "border-right:1px groove rgb(111,156,211)}"
        "QPushButton:focus{background-color:rgb(111,156,211)}"
    );
    ui->confirmButton->setStyleSheet(
        "QPushButton{background-color:white;text-align:center;}"
        "QPushButton:focus{background-color:rgb(111,156,211)}"
    );
    ui->cancelButton->setAutoDefault(false);
    ui->confirmButton->setAutoDefault(false);
    ui->cancelButton->setFocusPolicy(Qt::NoFocus);
    ui->confirmButton->setFocusPolicy(Qt::NoFocus);

    // 创建 RollingBox
    m_rolling_box = new RollingBox(this);
    ui->Layout->setAlignment(Qt::AlignCenter);
    ui->Layout->addWidget(m_rolling_box);

    // 信号连接
    connect(
        ui->cancelButton,
        &QPushButton::released,
        this,
        &ChangeDateTimePage::on_cancel_clicked
    );
    connect(
        ui->confirmButton,
        &QPushButton::released,
        this,
        &ChangeDateTimePage::on_confirm_clicked
    );

    // 初始焦点
    m_rolling_box->setFocus();
    m_rolling_box->choose(0);
}

ChangeDateTimePage::~ChangeDateTimePage() { delete ui; }

void ChangeDateTimePage::refresh()
{
    // 重建 RollingBox 以加载最新时间
    if (m_rolling_box) {
        ui->Layout->removeWidget(m_rolling_box);
        m_rolling_box->deleteLater();
    }
    m_rolling_box = new RollingBox(this);
    ui->Layout->setAlignment(Qt::AlignCenter);
    ui->Layout->addWidget(m_rolling_box);
    m_btn_pos = 0;
    m_rolling_box->setFocus();
    m_rolling_box->choose(0);
}

void ChangeDateTimePage::translateUi()
{
    ui->titleLabel->setText(tr("Change Date&Time"));
    ui->cancelButton->setText(tr("Cancel"));
    ui->confirmButton->setText(tr("Confirm"));
}

void ChangeDateTimePage::on_cancel_clicked()
{
    LOG_INFO("ChangeDateTimePage: cancel clicked");
    emit back_clicked();
}

void ChangeDateTimePage::on_confirm_clicked()
{
    LOG_INFO("ChangeDateTimePage: confirm clicked");

    int year = m_rolling_box->readYear();
    int month = m_rolling_box->readMonth();
    int day = m_rolling_box->readDay();
    int hour = m_rolling_box->readHour();
    int minute = m_rolling_box->readMinute();
    int second = m_rolling_box->readSecond();

    // 格式: "MM/DD/YYYY HH:MM:SS"
    QString dateTimeStr = QString("\"%1/%2/%3 %4:%5:%6\"")
                              .arg(month, 2, 10, QChar('0'))
                              .arg(day, 2, 10, QChar('0'))
                              .arg(year, 4, 10, QChar('0'))
                              .arg(hour, 2, 10, QChar('0'))
                              .arg(minute, 2, 10, QChar('0'))
                              .arg(second, 2, 10, QChar('0'));

    QString dateCmd = QString("/bin/sh -c \"date -s %1\"").arg(dateTimeStr);
    LOG_INFO("Setting date: {}", dateCmd.toStdString());
    QProcess::execute(dateCmd);

    QString hwclockCmd = QString("/bin/sh -c \"hwclock -w\"");
    LOG_INFO("Syncing to hardware clock");
    QProcess::execute(hwclockCmd);

    emit back_clicked();
}

void ChangeDateTimePage::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Return) {
        m_btn_pos++;
        m_btn_pos = m_btn_pos % m_btn_num;

        if (m_btn_pos >= 0 && m_btn_pos < 6) {
            m_rolling_box->choose(m_btn_pos);
            m_rolling_box->setFocus();
        } else {
            m_rolling_box->choose(-1);
            if (m_btn_pos == 6)
                ui->cancelButton->setFocus();
            else if (m_btn_pos == 7)
                ui->confirmButton->setFocus();
        }
        event->accept();
    } else if (event->key() == Qt::Key_Space) {
        if (m_btn_pos == 6)
            on_cancel_clicked();
        else if (m_btn_pos == 7)
            on_confirm_clicked();
        event->accept();
    } else if (event->key() == Qt::Key_Up) {
        if (m_btn_pos >= 0 && m_btn_pos < 6)
            m_rolling_box->changeTimebox(-1);
        event->accept();
    } else if (event->key() == Qt::Key_Down) {
        if (m_btn_pos >= 0 && m_btn_pos < 6)
            m_rolling_box->changeTimebox(1);
        event->accept();
    }
}

bool ChangeDateTimePage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (obj == ui->titleLabel) {
            LOG_INFO("ChangeDateTimePage: back from header");
            emit back_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
