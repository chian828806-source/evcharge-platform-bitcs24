#include "adminmanagementservice.h"

#include "shared/protocol/errorcodes.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QTimer>
#include <utility>

AdminManagementService::AdminManagementService(QSqlDatabase database)
    : m_database(std::move(database))
{
}

ResponseMessage AdminManagementService::restartPile(const RequestMessage &request,
                                                     qint64 adminId) const
{
    const qint64 pileId = request.payload.value(QStringLiteral("pileId")).toInteger(0);
    QSqlQuery find(m_database);
    find.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id = :id"));
    find.bindValue(QStringLiteral(":id"), pileId);
    if (!find.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      find.lastError().text());
    }
    if (!find.next()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::PileNotFound,
                                      QStringLiteral("charging pile not found"));
    }
    const QString previousStatus = find.value(0).toString();
    if (previousStatus == QStringLiteral("RESERVED")
        || previousStatus == QStringLiteral("CHARGING")
        || previousStatus == QStringLiteral("RESTARTING")) {
        return ResponseMessage::error(request.requestId, ErrorCodes::PileUnavailable,
                                      QStringLiteral("current pile status cannot be restarted"));
    }
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral("UPDATE charging_pile SET status='RESTARTING', updated_at=:now WHERE id=:id AND status=:status"));
    update.bindValue(QStringLiteral(":now"), now);
    update.bindValue(QStringLiteral(":id"), pileId);
    update.bindValue(QStringLiteral(":status"), previousStatus);
    QSqlQuery log(m_database);
    log.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, before_status, after_status, result, message, created_at) VALUES(:adminId, 'PILE_RESTART', 'PILE', :pileId, :before, 'RESTARTING', 'SUCCESS', '远程重启指令已发送', :now)"));
    log.bindValue(QStringLiteral(":adminId"), adminId);
    log.bindValue(QStringLiteral(":pileId"), pileId);
    log.bindValue(QStringLiteral(":before"), previousStatus);
    log.bindValue(QStringLiteral(":now"), now);
    if (!update.exec() || update.numRowsAffected() != 1 || !log.exec()
        || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }

    const QSqlDatabase database = m_database;
    QTimer::singleShot(1500, [database, pileId, previousStatus, adminId]() mutable {
        const QString completedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!database.transaction()) {
            return;
        }
        QSqlQuery restore(database);
        restore.prepare(QStringLiteral("UPDATE charging_pile SET status=:status, updated_at=:now WHERE id=:id AND status='RESTARTING'"));
        restore.bindValue(QStringLiteral(":status"), previousStatus);
        restore.bindValue(QStringLiteral(":now"), completedAt);
        restore.bindValue(QStringLiteral(":id"), pileId);
        QSqlQuery completedLog(database);
        completedLog.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, before_status, after_status, result, message, created_at) VALUES(:adminId, 'PILE_RESTART', 'PILE', :pileId, 'RESTARTING', :after, 'SUCCESS', '远程重启模拟完成', :now)"));
        completedLog.bindValue(QStringLiteral(":adminId"), adminId);
        completedLog.bindValue(QStringLiteral(":pileId"), pileId);
        completedLog.bindValue(QStringLiteral(":after"), previousStatus);
        completedLog.bindValue(QStringLiteral(":now"), completedAt);
        if (!restore.exec() || restore.numRowsAffected() != 1 || !completedLog.exec()) {
            database.rollback();
            return;
        }
        database.commit();
    });
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("pileId"), pileId},
        {QStringLiteral("status"), QStringLiteral("RESTARTING")},
        {QStringLiteral("restoreStatus"), previousStatus}
    });
}

ResponseMessage AdminManagementService::pileList(const RequestMessage &request) const
{
    const qint64 stationId = request.payload.value(QStringLiteral("stationId")).toInteger(0);
    QSqlQuery query(m_database);
    QString sql = QStringLiteral(
        "SELECT p.id, p.pile_no, s.id, s.name, p.type, p.power_kw, p.status, "
        "p.total_charge_count, p.total_charge_minutes "
        "FROM charging_pile p JOIN charging_station s ON s.id = p.station_id");
    if (stationId > 0) {
        sql += QStringLiteral(" WHERE s.id = :stationId");
    }
    sql += QStringLiteral(" ORDER BY s.station_no, p.pile_no");
    query.prepare(sql);
    if (stationId > 0) {
        query.bindValue(QStringLiteral(":stationId"), stationId);
    }
    if (!query.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QJsonArray piles;
    while (query.next()) {
        piles.append(QJsonObject{
            {QStringLiteral("pileId"), query.value(0).toLongLong()},
            {QStringLiteral("pileNo"), query.value(1).toString()},
            {QStringLiteral("stationId"), query.value(2).toLongLong()},
            {QStringLiteral("stationName"), query.value(3).toString()},
            {QStringLiteral("type"), query.value(4).toString()},
            {QStringLiteral("powerKw"), query.value(5).toDouble()},
            {QStringLiteral("status"), query.value(6).toString()},
            {QStringLiteral("totalChargeCount"), query.value(7).toInt()},
            {QStringLiteral("totalChargeMinutes"), query.value(8).toInt()}
        });
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("piles"), piles}});
}

ResponseMessage AdminManagementService::stationList(const RequestMessage &request) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.station_no, s.name, s.address, s.longitude, s.latitude, "
            "COUNT(p.id), COALESCE(SUM(CASE WHEN p.status <> 'OFFLINE' THEN 1 ELSE 0 END), 0) "
            "FROM charging_station s LEFT JOIN charging_pile p ON p.station_id=s.id "
            "GROUP BY s.id ORDER BY s.station_no"))) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QJsonArray stations;
    while (query.next()) {
        const int pileCount = query.value(6).toInt();
        stations.append(QJsonObject{
            {QStringLiteral("stationId"), query.value(0).toLongLong()},
            {QStringLiteral("stationNo"), query.value(1).toString()},
            {QStringLiteral("name"), query.value(2).toString()},
            {QStringLiteral("address"), query.value(3).toString()},
            {QStringLiteral("longitude"), query.value(4).toDouble()},
            {QStringLiteral("latitude"), query.value(5).toDouble()},
            {QStringLiteral("pileCount"), pileCount},
            {QStringLiteral("onlineRate"), pileCount > 0
                 ? query.value(7).toDouble() / pileCount : 0.0}
        });
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("stations"), stations}});
}
