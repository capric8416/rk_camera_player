#include "stdin_cmd.hpp"
#include <unistd.h>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTextStream>
#include "main_window.hpp"
#include "player/logger.hpp"

StdinCommandHandler::StdinCommandHandler(
    MainWindow *main_window,
    QObject *parent
)
    : QObject(parent)
    , m_main_window(main_window)
    , m_stdin_notifier(nullptr)
{
    m_stdin_notifier =
        new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(
        m_stdin_notifier,
        SIGNAL(activated(int)),
        this,
        SLOT(on_stdin_ready())
    );
    LOG_INFO("Stdin command listener started. Type 'help' for commands.");
}

StdinCommandHandler::~StdinCommandHandler() {}

void StdinCommandHandler::on_stdin_ready()
{
    QTextStream stream(stdin);
    QString line = stream.readLine().trimmed();

    if (!line.isEmpty()) {
        process_command(line);
        emit command_received(line);
    }
}

void StdinCommandHandler::process_command(const QString &cmd)
{
    LOG_INFO("Command received: {}", cmd.toStdString());

    if (cmd == "start_camera") {
        m_main_window->start_camera();
        LOG_INFO("Camera started");
    } else if (cmd == "stop_camera") {
        m_main_window->stop_camera();
        LOG_INFO("Camera stopped");
    } else if (cmd == "capture_image") {
        m_main_window->capture_image();
    } else if (cmd == "start_video_capture") {
        m_main_window->start_video_capture();
    } else if (cmd == "stop_video_capture") {
        m_main_window->stop_video_capture();
    } else if (cmd == "show_overlay") {
        m_main_window->show_camera_overlay_page();
    } else if (cmd == "show_launcher") {
        m_main_window->show_launcher_page();
    } else if (cmd == "quit" || cmd == "exit") {
        LOG_INFO("Exiting...");
        m_main_window->stop_camera();
        QCoreApplication::quit();
    } else if (cmd == "help") {
        LOG_INFO("Available commands:");
        LOG_INFO("  start_camera        - Start camera capture");
        LOG_INFO("  stop_camera         - Stop camera capture");
        LOG_INFO("  capture_image       - Capture a photo");
        LOG_INFO("  start_video_capture - Start video recording");
        LOG_INFO("  stop_video_capture  - Stop video recording");
        LOG_INFO("  show_overlay        - Switch to overlay page");
        LOG_INFO("  show_launcher       - Switch to launcher page");
        LOG_INFO("  quit/exit           - Exit application");
        LOG_INFO("  help                - Show this help");
    } else {
        LOG_WARN(
            "Unknown command: {}. Type 'help' for available commands.",
            cmd.toStdString()
        );
    }
}
