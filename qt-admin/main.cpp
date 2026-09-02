#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("EVCharge 管理端"));
    MainWindow window;
    window.show();
    return application.exec();
}
