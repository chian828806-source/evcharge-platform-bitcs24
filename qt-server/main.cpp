/* The sole composition root: one database, dispatcher and TCP server. */
#include "database/databasemanager.h"
#include "handlers/admin/registeradminhandlers.h"
#include "handlers/user/registeruserbackend.h"
#include "handlers/prediction/registerpredictionhandlers.h"
#include "network/dashboardwebsocketserver.h"
#include "network/messagedispatcher.h"
#include "network/sessionmanager.h"
#include "network/socketserver.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace {

QString defaultDatabasePath()
{
    const QString relativePath = QStringLiteral("database/evcharge.db");
    const QList<QString> starts = {QDir::currentPath(), QCoreApplication::applicationDirPath()};
    for (const QString &start : starts) {
        QDir directory(start);
        while (true) {
            const QString candidate = directory.filePath(relativePath);
            if (QFileInfo::exists(candidate)) {
                return QDir::cleanPath(candidate);
            }
            if (!directory.cdUp()) {
                break;
            }
        }
    }
    return QDir::cleanPath(QDir::current().filePath(relativePath));
}

bool hasRequiredTables(QSqlDatabase &database, QString *errorMessage)
{
    const QStringList requiredTables = {
        QStringLiteral("user"), QStringLiteral("admin"),
        QStringLiteral("charging_station"), QStringLiteral("charging_pile"),
        QStringLiteral("charging_order"), QStringLiteral("recharge_record"),
        QStringLiteral("prediction_batch"), QStringLiteral("prediction"),
        QStringLiteral("operation_log"), QStringLiteral("data_import_batch"),
        QStringLiteral("charging_session_history"),
        QStringLiteral("station_hourly_metric")};
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = :tableName"));
    for (const QString &table : requiredTables) {
        query.bindValue(QStringLiteral(":tableName"), table);
        if (!query.exec() || !query.next()) {
            if (errorMessage) {
                *errorMessage = query.lastError().isValid()
                    ? query.lastError().text()
                    : QStringLiteral("required table is missing: %1").arg(table);
            }
            return false;
        }
    }
    return true;
}

} // namespace

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
                      QStringLiteral("SQLite database path"), QStringLiteral("path"),
                      defaultDatabasePath()});
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
    if (!hasRequiredTables(database, &error)) {
        QTextStream(stderr) << "SQLite schema is unavailable: " << error << '\n';
        return 3;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserBackendRegistry userHandlers(&databaseManager, &sessions, &dispatcher);
    AdminHandlerRegistry adminHandlers(database, &sessions, &dispatcher);
    PredictionHandlerRegistry predictionHandlers(&databaseManager, &dispatcher);
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
