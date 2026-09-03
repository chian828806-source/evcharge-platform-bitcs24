#include "ui/userwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("EVCharge User"));
    application.setOrganizationName(QStringLiteral("BITCS24"));

    QFile styleFile(QStringLiteral(":/styles/user.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    UserWindow window;
    window.show();
    return application.exec();
}

