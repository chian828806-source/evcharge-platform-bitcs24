#include "adminmanagementservice.h"
#include "shared/protocol/errorcodes.h"
#include <QDateTime>
#include <QSqlError>
#include <QTimer>

namespace {
QString nowText() { return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")); }
ResponseMessage databaseError(const RequestMessage &request, const QString &error)
{ return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError, error); }
}

AdminManagementService::AdminManagementService(QSqlDatabase database, QObject *parent)
    : QObject(parent), m_database(database), m_pileRepository(database),
      m_stationRepository(database), m_userRepository(database),
      m_logRepository(database)
{
}

ResponseMessage AdminManagementService::pileList(const RequestMessage &request) const
{
    m_pileRepository.clearError();
    const QJsonArray piles = m_pileRepository.list(
        request.payload.value(QStringLiteral("stationId")).toInteger());
    if (!m_pileRepository.lastError().isEmpty()) return databaseError(request, m_pileRepository.lastError());
    return ResponseMessage::success(request.requestId, {{QStringLiteral("piles"), piles}});
}

ResponseMessage AdminManagementService::stationList(const RequestMessage &request) const
{
    m_stationRepository.clearError();
    const QJsonArray stations = m_stationRepository.list();
    if (!m_stationRepository.lastError().isEmpty()) return databaseError(request, m_stationRepository.lastError());
    return ResponseMessage::success(request.requestId, {{QStringLiteral("stations"), stations}});
}

ResponseMessage AdminManagementService::userList(const RequestMessage &request) const
{
    m_userRepository.clearError();
    const QJsonArray users = m_userRepository.list(
        request.payload.value(QStringLiteral("phoneKeyword")).toString());
    if (!m_userRepository.lastError().isEmpty()) return databaseError(request, m_userRepository.lastError());
    return ResponseMessage::success(request.requestId, {{QStringLiteral("users"), users}});
}

ResponseMessage AdminManagementService::restartPile(const RequestMessage &request,
                                                     qint64 adminId) const
{
    const qint64 pileId = request.payload.value(QStringLiteral("pileId")).toInteger();
    m_pileRepository.clearError(); m_logRepository.clearError();
    bool found = false; const QString before = m_pileRepository.status(pileId, &found);
    if (!m_pileRepository.lastError().isEmpty()) return databaseError(request, m_pileRepository.lastError());
    if (!found) return ResponseMessage::error(request.requestId, ErrorCodes::PileNotFound, QStringLiteral("charging pile not found"));
    if (before == QStringLiteral("RESERVED") || before == QStringLiteral("CHARGING") || before == QStringLiteral("RESTARTING"))
        return ResponseMessage::error(request.requestId, ErrorCodes::PileUnavailable, QStringLiteral("current pile status cannot be restarted"));
    const QString now = nowText();
    if (!m_database.transaction()) return databaseError(request, m_database.lastError().text());
    if (!m_pileRepository.compareAndSetStatus(pileId, before, QStringLiteral("RESTARTING"), now)
        || !m_logRepository.add(adminId, QStringLiteral("PILE_RESTART"), QStringLiteral("PILE"), pileId, before, QStringLiteral("RESTARTING"), QStringLiteral("远程重启指令已发送"), now)
        || !m_database.commit()) {
        m_database.rollback(); return databaseError(request, m_pileRepository.lastError() + m_logRepository.lastError());
    }
    QTimer::singleShot(1500, this, [this, pileId, before, adminId]() {
        const QString completed = nowText(); if (!m_database.transaction()) return;
        if (!m_pileRepository.compareAndSetStatus(pileId, QStringLiteral("RESTARTING"), before, completed)
            || !m_logRepository.add(adminId, QStringLiteral("PILE_RESTART"), QStringLiteral("PILE"), pileId, QStringLiteral("RESTARTING"), before, QStringLiteral("远程重启模拟完成"), completed)
            || !m_database.commit()) m_database.rollback();
    });
    return ResponseMessage::success(request.requestId, {{QStringLiteral("pileId"), pileId}, {QStringLiteral("status"), QStringLiteral("RESTARTING")}, {QStringLiteral("restoreStatus"), before}});
}

ResponseMessage AdminManagementService::createStation(const RequestMessage &request,
                                                       qint64 adminId) const
{
    const QJsonObject payload = request.payload;
    m_stationRepository.clearError(); m_pileRepository.clearError();
    m_logRepository.clearError();
    const QString name = payload.value(QStringLiteral("name")).toString().trimmed();
    const QString address = payload.value(QStringLiteral("address")).toString().trimmed();
    const double longitude = payload.value(QStringLiteral("longitude")).toDouble(999.0);
    const double latitude = payload.value(QStringLiteral("latitude")).toDouble(999.0);
    const int count = payload.value(QStringLiteral("pileCount")).toInt();
    if (name.isEmpty() || address.isEmpty() || longitude < -180 || longitude > 180
        || latitude < -90 || latitude > 90 || count < 1 || count > 100)
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid station fields"));
    const QString now = nowText(); const QString stationNo = QStringLiteral("ST%1").arg(QDateTime::currentMSecsSinceEpoch());
    if (!m_database.transaction()) return databaseError(request, m_database.lastError().text());
    const qint64 stationId = m_stationRepository.create(payload, stationNo, now);
    if (!stationId || !m_pileRepository.createForStation(stationId, count, now)
        || !m_logRepository.add(adminId, QStringLiteral("STATION_CREATE"), QStringLiteral("STATION"), stationId, {}, {}, QStringLiteral("新增充电站并生成 %1 个模拟电桩").arg(count), now)
        || !m_database.commit()) {
        m_database.rollback(); return databaseError(request, m_stationRepository.lastError() + m_pileRepository.lastError() + m_logRepository.lastError());
    }
    return ResponseMessage::success(request.requestId, {{QStringLiteral("stationId"), stationId}, {QStringLiteral("stationNo"), stationNo}, {QStringLiteral("pileCount"), count}});
}

ResponseMessage AdminManagementService::setUserFrozen(const RequestMessage &request,
                                                       qint64 adminId, bool frozen) const
{
    const qint64 userId = request.payload.value(QStringLiteral("userId")).toInteger();
    m_userRepository.clearError(); m_logRepository.clearError();
    const QJsonObject user = m_userRepository.findById(userId);
    if (!m_userRepository.lastError().isEmpty()) return databaseError(request, m_userRepository.lastError());
    if (user.isEmpty()) return ResponseMessage::error(request.requestId, ErrorCodes::InvalidPhone, QStringLiteral("user not found"));
    const QString before = user.value(QStringLiteral("status")).toString();
    const QString after = frozen ? QStringLiteral("FROZEN") : QStringLiteral("NORMAL");
    if (before == after) return ResponseMessage::success(request.requestId, {{QStringLiteral("userId"), userId}, {QStringLiteral("status"), after}, {QStringLiteral("changed"), false}});
    const QString now = nowText(); if (!m_database.transaction()) return databaseError(request, m_database.lastError().text());
    const QString action = frozen ? QStringLiteral("USER_FREEZE") : QStringLiteral("USER_UNFREEZE");
    const QString message = (frozen ? QStringLiteral("冻结用户 ") : QStringLiteral("解冻用户 ")) + user.value(QStringLiteral("phone")).toString();
    if (!m_userRepository.compareAndSetStatus(userId, before, after, now)
        || !m_logRepository.add(adminId, action, QStringLiteral("USER"), userId, before, after, message, now)
        || !m_database.commit()) {
        m_database.rollback(); return databaseError(request, m_userRepository.lastError() + m_logRepository.lastError());
    }
    return ResponseMessage::success(request.requestId, {{QStringLiteral("userId"), userId}, {QStringLiteral("status"), after}, {QStringLiteral("changed"), true}});
}
