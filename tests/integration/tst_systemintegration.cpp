/*
 * 功能：从真实TCP客户端验证统一Server装配、角色边界和跨模块共享SQLite。
 * 每一个测试拥有独立临时目录、数据库、端口和服务端进程。
 */
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/jsonlinecodec.h"
#include "shared/protocol/protocolmessage.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

namespace {
constexpr int TimeoutMs = 8000;
int failures = 0;

void check(bool condition, const QString &name, const QString &detail = {})
{
    QTextStream stream(condition ? stdout : stderr);
    stream << (condition ? "PASS " : "FAIL ") << name;
    if (!detail.isEmpty()) stream << ": " << detail;
    stream << '\n';
    if (!condition) ++failures;
}

quint16 unusedPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) return 0;
    return probe.serverPort();
}

QString executableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("evcharge-qt-server.exe");
#else
    return QStringLiteral("evcharge-qt-server");
#endif
}

QString serverBinary()
{
    const QString configured = qEnvironmentVariable("EVCHARGE_SERVER_BINARY");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) return configured;
    const QDir root(QStringLiteral(EVCHARGE_REPO_ROOT));
    const QString name = executableName();
    const QStringList candidates = {
        root.filePath(QStringLiteral("qt-server/") + name),
        root.filePath(QStringLiteral("qt-server/release/") + name),
        root.filePath(QStringLiteral("qt-server/debug/") + name),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../qt-server/release/") + name),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../qt-server/debug/") + name)
    };
    for (const QString &candidate : candidates) if (QFileInfo::exists(candidate)) return candidate;
    return {};
}

bool executeSqlFile(QSqlDatabase &database, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = file.errorString(); return false;
    }
    QString sql;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine());
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('.'))) continue;
        // 当前仓库SQL不在字符串字面量中使用--；先移除行注释，避免分号后的纯注释段被QSqlQuery执行。
        sql += line.left(line.indexOf(QStringLiteral("--"))) + QLatin1Char('\n');
    }
    const QStringList statements = sql.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QSqlQuery query(database);
    for (const QString &statement : statements) {
        if (statement.trimmed().isEmpty()) continue;
        if (!query.exec(statement)) {
            *error = QStringLiteral("%1: %2").arg(path, query.lastError().text());
            return false;
        }
    }
    return true;
}

class ServerFixture {
public:
    ~ServerFixture() { stop(); }

    bool start(QString *error)
    {
        if (serverBinary().isEmpty()) {
            *error = QStringLiteral("server binary not found; set EVCHARGE_SERVER_BINARY"); return false;
        }
        if (!m_dir.isValid()) { *error = QStringLiteral("temporary directory unavailable"); return false; }
        m_databasePath = m_dir.filePath(QStringLiteral("integration.sqlite"));
        const QString connectionName = QStringLiteral("integration-init-%1").arg(reinterpret_cast<quintptr>(this));
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(m_databasePath);
            if (!database.open()) { *error = database.lastError().text(); return false; }
            const QDir root(QStringLiteral(EVCHARGE_REPO_ROOT));
            if (!executeSqlFile(database, root.filePath(QStringLiteral("database/schema.sql")), error)
                || !executeSqlFile(database, root.filePath(QStringLiteral("database/init_data.sql")), error)) return false;
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
        m_tcpPort = unusedPort();
        m_webSocketPort = unusedPort();
        m_devicePort = unusedPort();
        if (!m_tcpPort || !m_webSocketPort || !m_devicePort
            || m_tcpPort == m_webSocketPort || m_tcpPort == m_devicePort
            || m_webSocketPort == m_devicePort) {
            *error = QStringLiteral("could not reserve distinct ephemeral ports"); return false;
        }
        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.start(serverBinary(), {QStringLiteral("--tcp-port"), QString::number(m_tcpPort),
                                         QStringLiteral("--websocket-port"), QString::number(m_webSocketPort),
                                         QStringLiteral("--device-port"), QString::number(m_devicePort),
                                         QStringLiteral("--database"), m_databasePath,
                                         QStringLiteral("--avatar-dir"), m_dir.filePath(QStringLiteral("avatars"))});
        if (!m_process.waitForStarted(TimeoutMs)) { *error = m_process.errorString(); return false; }
        QElapsedTimer readyDeadline;
        readyDeadline.start();
        QString socketError;
        while (readyDeadline.elapsed() < TimeoutMs && m_process.state() != QProcess::NotRunning) {
            QTcpSocket probe;
            probe.connectToHost(QHostAddress::LocalHost, m_tcpPort);
            if (probe.waitForConnected(250)) {
                probe.disconnectFromHost();
                return true;
            }
            socketError = probe.errorString();
            QThread::msleep(50);
        }
        *error = QString::fromUtf8(m_process.readAll()) + QStringLiteral(" / ") + socketError;
        return false;
    }

    void stop()
    {
        if (m_process.state() == QProcess::NotRunning) return;
        m_process.terminate();
        if (!m_process.waitForFinished(3000)) { m_process.kill(); m_process.waitForFinished(3000); }
    }

    QJsonObject request(const QString &type, const QString &sessionId, const QJsonObject &payload,
                        QString *error) const
    {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, m_tcpPort);
        if (!socket.waitForConnected(TimeoutMs)) { *error = socket.errorString(); return {}; }
        const RequestMessage message {QStringLiteral("integration-%1").arg(++m_requestId), type, sessionId, payload};
        socket.write(JsonLineCodec::encode(message.toJson()));
        if (!socket.waitForReadyRead(TimeoutMs)) { *error = socket.errorString(); return {}; }
        const QJsonDocument response = QJsonDocument::fromJson(socket.readLine().trimmed());
        if (!response.isObject()) { *error = QStringLiteral("server response is not JSON object"); return {}; }
        return response.object();
    }

    quint16 tcpPort() const { return m_tcpPort; }
    bool running() const { return m_process.state() != QProcess::NotRunning; }

private:
    QTemporaryDir m_dir;
    QString m_databasePath;
    quint16 m_tcpPort = 0;
    quint16 m_webSocketPort = 0;
    quint16 m_devicePort = 0;
    mutable int m_requestId = 0;
    QProcess m_process;
};

QString login(ServerFixture &fixture, bool admin, QString *error)
{
    const QJsonObject response = fixture.request(admin ? QStringLiteral("ADMIN_LOGIN") : QStringLiteral("USER_LOGIN"), {},
        admin ? QJsonObject{{QStringLiteral("username"), QStringLiteral("admin")}, {QStringLiteral("password"), QStringLiteral("123456")}}
              : QJsonObject{{QStringLiteral("phone"), QStringLiteral("13900000001")}}, error);
    if (response.value(QStringLiteral("code")).toInt() != ErrorCodes::Success) {
        *error = response.value(QStringLiteral("message")).toString(); return {};
    }
    return response.value(QStringLiteral("data")).toObject().value(QStringLiteral("sessionId")).toString();
}

void startupSmoke()
{
    ServerFixture fixture; QString error;
    const bool started = fixture.start(&error);
    check(started && fixture.running() && fixture.tcpPort() != 0,
          QStringLiteral("Unified Server Startup / Composition Smoke Test"), error);
}

void roleBoundary()
{
    ServerFixture fixture; QString error;
    if (!fixture.start(&error)) { check(false, QStringLiteral("User/Admin Routing Boundary Integration Test"), error); return; }
    const QString userSession = login(fixture, false, &error);
    const QJsonObject denied = userSession.isEmpty() ? QJsonObject() : fixture.request(QStringLiteral("ADMIN_STATION_LIST"), userSession, {}, &error);
    const QString adminSession = login(fixture, true, &error);
    const QJsonObject allowed = adminSession.isEmpty() ? QJsonObject() : fixture.request(QStringLiteral("ADMIN_STATION_LIST"), adminSession, {}, &error);
    check(!userSession.isEmpty() && denied.value(QStringLiteral("code")).toInt() == ErrorCodes::InvalidSession
          && !adminSession.isEmpty() && allowed.value(QStringLiteral("code")).toInt() == ErrorCodes::Success
          && allowed.value(QStringLiteral("data")).toObject().value(QStringLiteral("stations")).isArray(),
          QStringLiteral("User/Admin Routing Boundary Integration Test"), error);
}

void sharedDatabase()
{
    ServerFixture fixture; QString error;
    if (!fixture.start(&error)) { check(false, QStringLiteral("Shared Database Cross-Module Integration Test"), error); return; }
    const QString adminSession = login(fixture, true, &error);
    const QString name = QStringLiteral("Integration station %1").arg(QCoreApplication::applicationPid());
    const QJsonObject created = adminSession.isEmpty() ? QJsonObject() : fixture.request(QStringLiteral("ADMIN_STATION_CREATE"), adminSession,
        {{QStringLiteral("name"), name}, {QStringLiteral("address"), QStringLiteral("Integration test address")},
         {QStringLiteral("longitude"), 121.500001}, {QStringLiteral("latitude"), 38.860001},
         {QStringLiteral("pileCount"), 2}, {QStringLiteral("priceFenPerKwh"), 120}}, &error);
    const qint64 stationId = created.value(QStringLiteral("data")).toObject().value(QStringLiteral("stationId")).toVariant().toLongLong();
    const QString userSession = login(fixture, false, &error);
    const QJsonObject listed = userSession.isEmpty() ? QJsonObject() : fixture.request(QStringLiteral("STATION_LIST_NEARBY"), userSession,
        {{QStringLiteral("longitude"), 121.500001}, {QStringLiteral("latitude"), 38.860001}, {QStringLiteral("limit"), 20}}, &error);
    bool found = false;
    for (const QJsonValue &item : listed.value(QStringLiteral("data")).toObject().value(QStringLiteral("stations")).toArray()) {
        const QJsonObject station = item.toObject();
        if (station.value(QStringLiteral("stationId")).toVariant().toLongLong() == stationId && station.value(QStringLiteral("name")).toString() == name) { found = true; break; }
    }
    check(!adminSession.isEmpty() && created.value(QStringLiteral("code")).toInt() == ErrorCodes::Success && stationId > 0
          && !userSession.isEmpty() && listed.value(QStringLiteral("code")).toInt() == ErrorCodes::Success && found,
          QStringLiteral("Shared Database Cross-Module Integration Test"), error);
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    startupSmoke();
    roleBoundary();
    sharedDatabase();
    QTextStream(stdout) << "TOTAL_FAILURES " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
