#ifndef STDIN_CMD_H
#define STDIN_CMD_H

#include <QObject>

class QSocketNotifier;
class MainWindow;

/**
 * StdinCommandHandler - 处理标准输入命令
 */
class StdinCommandHandler : public QObject
{
    Q_OBJECT
public:
    explicit StdinCommandHandler(MainWindow *main_window, QObject *parent = nullptr);
    ~StdinCommandHandler();

signals:
    void command_received(const QString &cmd);

private slots:
    void on_stdin_ready();

private:
    void process_command(const QString &cmd);
    
    MainWindow *m_main_window;
    QSocketNotifier *m_stdin_notifier;
};

#endif // STDIN_CMD_H
