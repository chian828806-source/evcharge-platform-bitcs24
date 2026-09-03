#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("EVCharge 管理端"));
    QFile styleFile(QStringLiteral(":/admin/admin.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
    MainWindow window;
    window.show();
    return application.exec();
}
