/* The sole composition root: one database, dispatcher and TCP server. */
#include "database/databasemanager.h"
#include "handlers/admin/registeradminhandlers.h"
#include "handlers/user/registeruserbackend.h"
#include "network/dashboardwebsocketserver.h"
#include "network/messagedispatcher.h"
#include "network/sessionmanager.h"
#include "network/socketserver.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("evcharge-qt-server"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("t"), QStringLiteral("tcp-port")},
                      QStringLiteral("TCP listen port"), QStringLiteral("port"), QStringLiteral("18080")});
    parser.addOption({{QStringLiteral("w"), QStringLiteral("websocket-port")},
                      QStringLiteral("WebSocket listen port"), QStringLiteral("port"), QStringLiteral("18081")});
    parser.addOption({{QStringLiteral("d"), QStringLiteral("database")},
                      QStringLiteral("SQLite database path"), QStringLiteral("path"), QStringLiteral("database/evcharge.db")});
    parser.process(application);

    bool tcpOk = false;
    bool websocketOk = false;
    const quint16 tcpPort = parser.value(QStringLiteral("tcp-port")).toUShort(&tcpOk);
    const quint16 websocketPort = parser.value(QStringLiteral("websocket-port")).toUShort(&websocketOk);
    if (!tcpOk || !websocketOk) {
        QTextStream(stderr) << "Invalid port value\n";
        return 2;
    }
    DatabaseManager databaseManager(parser.value(QStringLiteral("database")));
    QSqlDatabase database;
    QString error;
    if (!databaseManager.database(&database, &error)) {
        QTextStream(stderr) << "SQLite open failed: " << error << '\n';
        return 3;
    }
    QSqlQuery schemaCheck(database);
    if (!schemaCheck.exec(QStringLiteral("SELECT 1 FROM user LIMIT 1"))) {
        QTextStream(stderr) << "SQLite schema is unavailable: " << schemaCheck.lastError().text() << '\n';
        return 3;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserBackendRegistry userHandlers(&databaseManager, &sessions, &dispatcher);
    AdminHandlerRegistry adminHandlers(database, &sessions, &dispatcher);
    SocketServer socketServer(&dispatcher);
    if (!socketServer.listen(QHostAddress::Any, tcpPort)) {
        QTextStream(stderr) << "TCP listen failed: " << socketServer.errorString() << '\n';
        return 1;
    }
    DashboardWebSocketServer dashboardServer;
    if (!dashboardServer.listen(websocketPort)) {
        QTextStream(stderr) << "WebSocket listen failed: " << dashboardServer.errorString() << '\n';
        return 1;
    }
    QTextStream(stdout) << "TCP listening on " << tcpPort << '\n'
                        << "WebSocket listening on " << websocketPort << " path /dashboard\n";
    return application.exec();
}
