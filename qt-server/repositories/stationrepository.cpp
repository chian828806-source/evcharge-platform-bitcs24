/*
 * 功能：实现站点摘要聚合、站点详情和电桩列表查询。
 */
#include "stationrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString stationSummarySql(const QString &whereClause)
{
    return QStringLiteral(
        "SELECT s.id, s.station_no, s.name, s.address, s.district, "
        "s.longitude, s.latitude, s.price_fen_per_kwh, "
        "s.service_fee_fen_per_kwh, COUNT(p.id), "
        "COALESCE(SUM(CASE WHEN p.status = 'AVAILABLE' THEN 1 ELSE 0 END), 0) "
        "FROM charging_station s "
        "LEFT JOIN charging_pile p ON p.station_id = s.id "
        "%1 "
        "GROUP BY s.id, s.station_no, s.name, s.address, s.district, "
        "s.longitude, s.latitude, s.price_fen_per_kwh, "
        "s.service_fee_fen_per_kwh").arg(whereClause);
}

}

QList<StationInfo> StationRepository::listEnabled(QSqlDatabase &database,
                                                   const QString &district,
                                                   QString *errorMessage) const
{
    const bool filterDistrict = !district.isEmpty();
    QSqlQuery query(database);
    query.prepare(stationSummarySql(filterDistrict
        ? QStringLiteral("WHERE s.status = 'NORMAL' AND s.district = :district")
        : QStringLiteral("WHERE s.status = 'NORMAL'")));
    if (filterDistrict) {
        query.bindValue(QStringLiteral(":district"), district);
    }
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }

    QList<StationInfo> stations;
    while (query.next()) {
        stations.append(mapStation(query));
    }
    return stations;
}

std::optional<StationInfo> StationRepository::findEnabledById(
    QSqlDatabase &database, qint64 stationId, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(stationSummarySql(
        QStringLiteral("WHERE s.status = 'NORMAL' AND s.id = :stationId")));
    query.bindValue(QStringLiteral(":stationId"), stationId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return mapStation(query);
}

QList<ChargingPileInfo> StationRepository::listPiles(QSqlDatabase &database,
                                                      qint64 stationId,
                                                      QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT id, station_id, pile_no, type, power_kw, status "
        "FROM charging_pile WHERE station_id = :stationId ORDER BY pile_no"));
    query.bindValue(QStringLiteral(":stationId"), stationId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }

    QList<ChargingPileInfo> piles;
    while (query.next()) {
        piles.append(mapPile(query));
    }
    return piles;
}

StationInfo StationRepository::mapStation(const QSqlQuery &query)
{
    StationInfo station;
    station.stationId = query.value(0).toLongLong();
    station.stationNo = query.value(1).toString();
    station.name = query.value(2).toString();
    station.address = query.value(3).toString();
    station.district = query.value(4).toString();
    station.longitude = query.value(5).toDouble();
    station.latitude = query.value(6).toDouble();
    station.priceFenPerKwh = query.value(7).toLongLong();
    station.serviceFeeFenPerKwh = query.value(8).toLongLong();
    station.pileCount = query.value(9).toInt();
    station.availablePileCount = query.value(10).toInt();
    return station;
}

ChargingPileInfo StationRepository::mapPile(const QSqlQuery &query)
{
    ChargingPileInfo pile;
    pile.pileId = query.value(0).toLongLong();
    pile.stationId = query.value(1).toLongLong();
    pile.pileNo = query.value(2).toString();
    pile.type = query.value(3).toString();
    pile.powerKw = query.value(4).toDouble();
    pile.status = query.value(5).toString();
    return pile;
}
