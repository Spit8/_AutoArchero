#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AutoArchero"));
    QApplication::setOrganizationName(QStringLiteral("AutoArchero"));

    MainWindow w;
    w.show();
    return app.exec();
}
