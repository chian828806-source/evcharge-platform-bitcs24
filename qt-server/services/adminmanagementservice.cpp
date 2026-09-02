#include "adminmanagementservice.h"

#include "shared/protocol/errorcodes.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <utility>

AdminManagementService::AdminManagementService(QSqlDatabase database)
    : m_database(std::move(database))
{
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
