/**
 * RV1126B Qt Widgets UI - Main Entry Point
 */

#include <QApplication>
#include "main_window.hpp"
#include "player/logger.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    player::init_logger("rv1126b_ui", "log");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
