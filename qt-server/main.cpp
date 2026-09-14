/* The sole composition root: one database, dispatcher and TCP server. */
#include "database/databasemanager.h"
#include "handlers/admin/registeradminhandlers.h"
#include "handlers/user/registeruserbackend.h"
#include "handlers/prediction/registerpredictionhandlers.h"
#include "network/dashboardwebsocketserver.h"
#include "services/dashboard/dashboarddataservice.h"
#include "network/messagedispatcher.h"
#include "network/sessionmanager.h"
#include "network/socketserver.h"
#include "devices/devicegatewayserver.h"
#include "devices/deviceregistry.h"
#include "devices/devicecontrolservice.h"
#include "shared/protocol/messagetypes.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTextStream>
#include <QTimer>

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
        QStringLiteral("charging_order"), QStringLiteral("coupon"),
        QStringLiteral("user_station_favorite"),
        QStringLiteral("membership_product"), QStringLiteral("membership_purchase"),
        QStringLiteral("recharge_record"),
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

bool ensureCouponSchema(QSqlDatabase &database, QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS coupon ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES user(id), "
            "discount_rate INTEGER NOT NULL DEFAULT 80, status TEXT NOT NULL DEFAULT 'AVAILABLE', "
            "order_id INTEGER, issued_by INTEGER NOT NULL REFERENCES admin(id), issued_at TEXT NOT NULL, "
            "used_at TEXT, CHECK(discount_rate > 0 AND discount_rate <= 100), "
            "CHECK(status IN ('AVAILABLE','LOCKED','USED')))"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_coupon_user_status ON coupon(user_id, status, issued_at DESC)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    QSet<QString> columns;
    if (!query.exec(QStringLiteral("PRAGMA table_info(charging_order)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    while (query.next()) columns.insert(query.value(1).toString());
    if (!columns.contains(QStringLiteral("coupon_id"))
        && !query.exec(QStringLiteral("ALTER TABLE charging_order ADD COLUMN coupon_id INTEGER REFERENCES coupon(id)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    if (!columns.contains(QStringLiteral("discount_rate"))
        && !query.exec(QStringLiteral("ALTER TABLE charging_order ADD COLUMN discount_rate INTEGER NOT NULL DEFAULT 100"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool ensureProfileFeatureSchema(QSqlDatabase &database, QString *errorMessage)
{
    QSqlQuery query(database);
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS user_station_favorite (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES user(id), station_id INTEGER NOT NULL REFERENCES charging_station(id), created_at TEXT NOT NULL, UNIQUE(user_id, station_id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_favorite_user_created ON user_station_favorite(user_id, created_at DESC)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS membership_product (id INTEGER PRIMARY KEY AUTOINCREMENT, product_no TEXT NOT NULL UNIQUE, name TEXT NOT NULL, card_type TEXT NOT NULL, duration_days INTEGER NOT NULL, sale_price_fen INTEGER NOT NULL, service_fee_discount_bps INTEGER NOT NULL DEFAULT 8000, status TEXT NOT NULL DEFAULT 'ON_SALE', created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS membership_purchase (id INTEGER PRIMARY KEY AUTOINCREMENT, purchase_no TEXT NOT NULL UNIQUE, user_id INTEGER NOT NULL REFERENCES user(id), product_id INTEGER NOT NULL REFERENCES membership_product(id), amount_fen INTEGER NOT NULL, balance_after_fen INTEGER NOT NULL, expires_at TEXT NOT NULL, created_at TEXT NOT NULL)"),
        QStringLiteral("INSERT OR IGNORE INTO membership_product(product_no,name,card_type,duration_days,sale_price_fen,service_fee_discount_bps,status,created_at,updated_at) VALUES('VIP-MONTH','VIP月卡','MONTH',30,64800,8000,'ON_SALE',datetime('now','localtime'),datetime('now','localtime'))"),
        QStringLiteral("INSERT OR IGNORE INTO membership_product(product_no,name,card_type,duration_days,sale_price_fen,service_fee_discount_bps,status,created_at,updated_at) VALUES('VIP-SEASON','VIP季卡','SEASON',90,99900,8000,'ON_SALE',datetime('now','localtime'),datetime('now','localtime'))")};
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }
    QSet<QString> columns;
    if (!query.exec(QStringLiteral("PRAGMA table_info(user)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    while (query.next()) columns.insert(query.value(1).toString());
    const QList<QPair<QString, QString>> additions{
        {QStringLiteral("is_member"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("membership_remaining_days"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("membership_expires_at"), QStringLiteral("TEXT")},
        {QStringLiteral("membership_discount_bps"), QStringLiteral("INTEGER NOT NULL DEFAULT 10000")}};
    for (const auto &addition : additions) {
        if (!columns.contains(addition.first)
            && !query.exec(QStringLiteral("ALTER TABLE user ADD COLUMN %1 %2")
                               .arg(addition.first, addition.second))) {
            if (errorMessage) *errorMessage = query.lastError().text();
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
    parser.addOption({QStringLiteral("device-port"), QStringLiteral("Device TCP listen port"),
                      QStringLiteral("port"), QStringLiteral("18082")});
    parser.addOption({{QStringLiteral("d"), QStringLiteral("database")},
                      QStringLiteral("SQLite database path"), QStringLiteral("path"),
                      defaultDatabasePath()});
    parser.addOption({
        {QStringLiteral("a"), QStringLiteral("avatar-dir")},
        QStringLiteral("Avatar storage directory; database stores relative paths only"),
        QStringLiteral("path"),
        QStringLiteral("data/avatars")
    });
    parser.addOption({QStringLiteral("tencent-map-key"),
                      QStringLiteral("Tencent Map API key; prefer TENCENT_MAP_KEY environment variable"),
                      QStringLiteral("key")});
    parser.addOption({QStringLiteral("tencent-map-sk"),
                      QStringLiteral("Tencent Map WebService signing secret; prefer TENCENT_MAP_SK environment variable"),
                      QStringLiteral("secret")});
    parser.process(application);

    bool tcpOk = false;
    bool websocketOk = false;
    bool deviceOk = false;
    const quint16 tcpPort = parser.value(QStringLiteral("tcp-port")).toUShort(&tcpOk);
    const quint16 websocketPort = parser.value(QStringLiteral("websocket-port")).toUShort(&websocketOk);
    const quint16 devicePort = parser.value(QStringLiteral("device-port")).toUShort(&deviceOk);
    if (!tcpOk || !websocketOk || !deviceOk) {
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
    if (!ensureCouponSchema(database, &error)
        || !ensureProfileFeatureSchema(database, &error)
        || !hasRequiredTables(database, &error)) {
        QTextStream(stderr) << "SQLite schema is unavailable: " << error << '\n';
        return 3;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    DeviceRegistry deviceRegistry;
    DeviceControlService deviceControl(&databaseManager, &deviceRegistry);
    const QString mapApiKey = parser.isSet(QStringLiteral("tencent-map-key"))
        ? parser.value(QStringLiteral("tencent-map-key"))
        : qEnvironmentVariable("TENCENT_MAP_KEY");
    const QString mapSigningSecret = parser.isSet(QStringLiteral("tencent-map-sk"))
        ? parser.value(QStringLiteral("tencent-map-sk"))
        : qEnvironmentVariable("TENCENT_MAP_SK");
    UserBackendRegistry userHandlers(&databaseManager, &sessions, &dispatcher,
                                     parser.value(QStringLiteral("avatar-dir")), mapApiKey,
                                     mapSigningSecret, &deviceControl);
    AdminHandlerRegistry adminHandlers(&databaseManager, &sessions, &dispatcher, &deviceRegistry,
                                       &deviceControl);
    PredictionHandlerRegistry predictionHandlers(&databaseManager, &dispatcher);
    SocketServer socketServer(&dispatcher);
    if (!socketServer.listen(QHostAddress::Any, tcpPort)) {
        QTextStream(stderr) << "TCP listen failed: " << socketServer.errorString() << '\n';
        return 1;
    }
    DeviceGatewayServer deviceServer(&databaseManager, &deviceRegistry, &deviceControl);
    if (!deviceServer.listen(QHostAddress::Any, devicePort)) {
        QTextStream(stderr) << "Device TCP listen failed: " << deviceServer.errorString() << '\n';
        return 1;
    }
    QObject::connect(&deviceRegistry, &DeviceRegistry::becameOffline, &deviceControl,
                     [&deviceControl](qint64 pileId) { deviceControl.handleOffline(pileId); });
    QTimer deviceHeartbeatWatchdog;
    deviceHeartbeatWatchdog.setInterval(1000);
    QObject::connect(&deviceHeartbeatWatchdog, &QTimer::timeout, [&deviceRegistry, &deviceControl]() {
        for (qint64 pileId : deviceRegistry.expiredPiles()) deviceControl.handleOffline(pileId);
    });
    deviceHeartbeatWatchdog.start();
    DashboardWebSocketServer dashboardServer;
    DashboardDataService dashboardData(&databaseManager);
    dashboardServer.setSnapshotProvider([&dashboardData](const QString &topic, QString *errorMessage) {
        const auto result = dashboardData.dataForTopic(topic);
        if (!result.ok && errorMessage) {
            *errorMessage = result.message;
        }
        return result.ok ? result.value : QJsonObject();
    });
    if (!dashboardServer.listen(websocketPort)) {
        QTextStream(stderr) << "WebSocket listen failed: " << dashboardServer.errorString() << '\n';
        return 1;
    }
    QTextStream(stdout) << "TCP listening on " << tcpPort << '\n'
                        << "WebSocket listening on " << websocketPort << " path /dashboard\n"
                        << "Device TCP listening on " << devicePort << '\n'
                        << "Database path: " << databaseManager.databasePath() << '\n';
    QTimer dashboardRefreshTimer;
    dashboardRefreshTimer.setInterval(3000);
    QObject::connect(&dashboardRefreshTimer, &QTimer::timeout, [&dashboardData, &dashboardServer]() {
        for (const QString &topic : MessageTypes::dashboardTopics()) {
            const auto result = dashboardData.dataForTopic(topic);
            if (result.ok) {
                dashboardServer.publish(topic, result.value);
            }
        }
    });
    dashboardRefreshTimer.start();
    return application.exec();
}
