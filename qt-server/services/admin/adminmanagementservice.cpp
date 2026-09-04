#include "adminmanagementservice.h"
#include "database/databasemanager.h"
#include "repositories/operationlogrepository.h"
#include "repositories/pilerepository.h"
#include "shared/protocol/errorcodes.h"
#include <QDateTime>
#include <QSqlError>
#include <QTimer>

namespace {
QString nowText() { return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")); }
ResponseMessage databaseError(const RequestMessage &request, const QString &error)
{ return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError, error); }
bool databaseFor(DatabaseManager *manager, QSqlDatabase *database, QString *error)
{ return manager && manager->database(database, error); }
}

AdminManagementService::AdminManagementService(DatabaseManager *databaseManager, QObject *parent)
    : QObject(parent), m_databaseManager(databaseManager)
{
}

ResponseMessage AdminManagementService::pileList(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error)) return databaseError(request, error);
    PileRepository pileRepository(database);
    const QJsonArray piles = pileRepository.list(
        static_cast<qint64>(request.payload.value(QStringLiteral("stationId")).toDouble()));
    if (!pileRepository.lastError().isEmpty()) return databaseError(request, pileRepository.lastError());
    return ResponseMessage::success(request.requestId, {{QStringLiteral("piles"), piles}});
}

ResponseMessage AdminManagementService::stationList(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error)) return databaseError(request, error);
    const QJsonArray stations = m_stationRepository.listForAdmin(database, &error);
    if (!error.isEmpty()) return databaseError(request, error);
    return ResponseMessage::success(request.requestId, {{QStringLiteral("stations"), stations}});
}

ResponseMessage AdminManagementService::userList(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error)) return databaseError(request, error);
    const QJsonArray users = m_userRepository.listForAdmin(
        database, request.payload.value(QStringLiteral("phoneKeyword")).toString(), &error);
    if (!error.isEmpty()) return databaseError(request, error);
    return ResponseMessage::success(request.requestId, {{QStringLiteral("users"), users}});
}

ResponseMessage AdminManagementService::restartPile(const RequestMessage &request,
                                                     qint64 adminId) const
{
    QSqlDatabase database; QString databaseMessage;
    if (!databaseFor(m_databaseManager, &database, &databaseMessage)) return databaseError(request, databaseMessage);
    PileRepository pileRepository(database);
    OperationLogRepository logRepository(database);
    const qint64 pileId = static_cast<qint64>(
        request.payload.value(QStringLiteral("pileId")).toDouble());
    bool found = false; const QString before = pileRepository.status(pileId, &found);
    if (!pileRepository.lastError().isEmpty()) return databaseError(request, pileRepository.lastError());
    if (!found) return ResponseMessage::error(request.requestId, ErrorCodes::PileNotFound, QStringLiteral("charging pile not found"));
    if (before == QStringLiteral("RESERVED") || before == QStringLiteral("CHARGING") || before == QStringLiteral("RESTARTING"))
        return ResponseMessage::error(request.requestId, ErrorCodes::PileUnavailable, QStringLiteral("current pile status cannot be restarted"));
    const QString now = nowText();
    if (!database.transaction()) return databaseError(request, database.lastError().text());
    if (!pileRepository.compareAndSetStatus(pileId, before, QStringLiteral("RESTARTING"), now)
        || !logRepository.add(adminId, QStringLiteral("PILE_RESTART"), QStringLiteral("PILE"), pileId, before, QStringLiteral("RESTARTING"), QStringLiteral("远程重启指令已发送"), now)
        || !database.commit()) {
        const QString error = pileRepository.lastError() + logRepository.lastError()
            + database.lastError().text();
        database.rollback(); return databaseError(request, error);
    }
    QTimer::singleShot(1500, this, [this, pileId, before, adminId]() {
        QSqlDatabase delayedDatabase; QString error;
        if (!databaseFor(m_databaseManager, &delayedDatabase, &error)) return;
        PileRepository delayedPileRepository(delayedDatabase);
        OperationLogRepository delayedLogRepository(delayedDatabase);
        const QString completed = nowText(); if (!delayedDatabase.transaction()) return;
        if (!delayedPileRepository.compareAndSetStatus(pileId, QStringLiteral("RESTARTING"), before, completed)
            || !delayedLogRepository.add(adminId, QStringLiteral("PILE_RESTART"), QStringLiteral("PILE"), pileId, QStringLiteral("RESTARTING"), before, QStringLiteral("远程重启模拟完成"), completed)
            || !delayedDatabase.commit()) delayedDatabase.rollback();
    });
    return ResponseMessage::success(request.requestId, {{QStringLiteral("pileId"), pileId}, {QStringLiteral("status"), QStringLiteral("RESTARTING")}, {QStringLiteral("restoreStatus"), before}});
}

ResponseMessage AdminManagementService::createStation(const RequestMessage &request,
                                                       qint64 adminId) const
{
    const QJsonObject payload = request.payload;
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error)) return databaseError(request, error);
    PileRepository pileRepository(database);
    OperationLogRepository logRepository(database);
    const QString name = payload.value(QStringLiteral("name")).toString().trimmed();
    const QString address = payload.value(QStringLiteral("address")).toString().trimmed();
    const double longitude = payload.value(QStringLiteral("longitude")).toDouble(999.0);
    const double latitude = payload.value(QStringLiteral("latitude")).toDouble(999.0);
    const int count = payload.value(QStringLiteral("pileCount")).toInt();
    const int priceFenPerKwh = payload.value(QStringLiteral("priceFenPerKwh")).toInt(120);
    if (name.isEmpty() || address.isEmpty() || longitude < -180 || longitude > 180
        || latitude < -90 || latitude > 90 || count < 1 || count > 100
        || priceFenPerKwh < 1 || priceFenPerKwh > 10000)
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid station fields"));
    const QString now = nowText(); const QString stationNo = QStringLiteral("ST%1").arg(QDateTime::currentMSecsSinceEpoch());
    if (!database.transaction()) return databaseError(request, database.lastError().text());
    const qint64 stationId = m_stationRepository.createForAdmin(database, payload, stationNo, now, &error);
    if (!stationId || !pileRepository.createForStation(stationId, count, now)
        || !logRepository.add(adminId, QStringLiteral("STATION_CREATE"), QStringLiteral("STATION"), stationId, {}, {}, QStringLiteral("新增充电站并生成 %1 个模拟电桩").arg(count), now)
        || !database.commit()) {
        error += pileRepository.lastError() + logRepository.lastError() + database.lastError().text();
        database.rollback(); return databaseError(request, error);
    }
    return ResponseMessage::success(request.requestId, {{QStringLiteral("stationId"), stationId}, {QStringLiteral("stationNo"), stationNo}, {QStringLiteral("pileCount"), count}});
}

ResponseMessage AdminManagementService::setUserFrozen(const RequestMessage &request,
                                                       qint64 adminId, bool frozen) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error)) return databaseError(request, error);
    OperationLogRepository logRepository(database);
    const qint64 userId = static_cast<qint64>(
        request.payload.value(QStringLiteral("userId")).toDouble());
    const QJsonObject user = m_userRepository.statusForAdmin(database, userId, &error);
    if (!error.isEmpty()) return databaseError(request, error);
    if (user.isEmpty()) return ResponseMessage::error(request.requestId, ErrorCodes::InvalidPhone, QStringLiteral("user not found"));
    const QString before = user.value(QStringLiteral("status")).toString();
    const QString after = frozen ? QStringLiteral("FROZEN") : QStringLiteral("NORMAL");
    if (before == after) return ResponseMessage::success(request.requestId, {{QStringLiteral("userId"), userId}, {QStringLiteral("status"), after}, {QStringLiteral("changed"), false}});
    const QString now = nowText(); if (!database.transaction()) return databaseError(request, database.lastError().text());
    const QString action = frozen ? QStringLiteral("USER_FREEZE") : QStringLiteral("USER_UNFREEZE");
    const QString message = (frozen ? QStringLiteral("冻结用户 ") : QStringLiteral("解冻用户 ")) + user.value(QStringLiteral("phone")).toString();
    if (!m_userRepository.compareAndSetStatus(database, userId, before, after, now, &error)
        || !logRepository.add(adminId, action, QStringLiteral("USER"), userId, before, after, message, now)
        || !database.commit()) {
        error += logRepository.lastError() + database.lastError().text();
        database.rollback(); return databaseError(request, error);
    }
    return ResponseMessage::success(request.requestId, {{QStringLiteral("userId"), userId}, {QStringLiteral("status"), after}, {QStringLiteral("changed"), true}});
}
