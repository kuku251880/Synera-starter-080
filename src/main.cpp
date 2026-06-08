#include <QApplication>
#include "gui/gamewindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("协同自走棋"));
    app.setApplicationVersion("1.0");

    GameWindow window;
    window.setWindowTitle(QStringLiteral("协同自走棋"));
    window.resize(900, 700);
    window.show();

    return app.exec();
}
