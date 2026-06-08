#include <QApplication>
#include "gui/gamewindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Synera Starter");
    app.setApplicationVersion("1.0");

    GameWindow window;
    window.setWindowTitle("Synera - Starter");
    window.resize(900, 700);
    window.show();

    return app.exec();
}
