/**
 * RV1126B Qt Widgets UI - Main Entry Point
 */

#include <QApplication>
#include "main_window.hpp"
#include "player/logger.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("skylink");
    QCoreApplication::setApplicationName("camera_player");

    player::init_logger("camera_player", "log");

    MainWindow window;
    window.show();

    return app.exec();
}
