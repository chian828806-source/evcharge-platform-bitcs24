#include "favoriterepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool FavoriteRepository::isFavorite(QSqlDatabase &database, qint64 userId,
                                    qint64 stationId, bool *favorite,
                                    QString *errorMessage) const
{
    if (!favorite) return false;
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM user_station_favorite "
        "WHERE user_id = :userId AND station_id = :stationId LIMIT 1"));
    query.bindValue(QStringLiteral(":userId"), userId);
    query.bindValue(QStringLiteral(":stationId"), stationId);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    *favorite = query.next();
    return true;
}

bool FavoriteRepository::insert(QSqlDatabase &database, qint64 userId,
                                qint64 stationId, const QString &createdAt,
                                QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO user_station_favorite(user_id, station_id, created_at) "
        "VALUES(:userId, :stationId, :createdAt)"));
    query.bindValue(QStringLiteral(":userId"), userId);
    query.bindValue(QStringLiteral(":stationId"), stationId);
    query.bindValue(QStringLiteral(":createdAt"), createdAt);
    if (query.exec()) return true;
    if (errorMessage) *errorMessage = query.lastError().text();
    return false;
}

bool FavoriteRepository::remove(QSqlDatabase &database, qint64 userId,
                                qint64 stationId, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "DELETE FROM user_station_favorite "
        "WHERE user_id = :userId AND station_id = :stationId"));
    query.bindValue(QStringLiteral(":userId"), userId);
    query.bindValue(QStringLiteral(":stationId"), stationId);
    if (query.exec()) return true;
    if (errorMessage) *errorMessage = query.lastError().text();
    return false;
}

QSet<qint64> FavoriteRepository::stationIds(QSqlDatabase &database, qint64 userId,
                                             QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT station_id FROM user_station_favorite WHERE user_id = :userId"));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return {};
    }
    QSet<qint64> result;
    while (query.next()) result.insert(query.value(0).toLongLong());
    return result;
}

QList<StationInfo> FavoriteRepository::listForUser(QSqlDatabase &database,
                                                    qint64 userId,
                                                    QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT s.id, s.station_no, s.name, s.address, s.district, s.longitude, "
        "s.latitude, s.price_fen_per_kwh, s.service_fee_fen_per_kwh, s.status, "
        "COUNT(p.id), COALESCE(SUM(CASE WHEN p.status='AVAILABLE' THEN 1 ELSE 0 END),0) "
        "FROM user_station_favorite f "
        "JOIN charging_station s ON s.id=f.station_id "
        "LEFT JOIN charging_pile p ON p.station_id=s.id "
        "WHERE f.user_id=:userId "
        "GROUP BY f.id, s.id ORDER BY f.created_at DESC, f.id DESC"));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return {};
    }
    QList<StationInfo> stations;
    while (query.next()) {
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
        station.status = query.value(9).toString();
        station.pileCount = query.value(10).toInt();
        station.availablePileCount = query.value(11).toInt();
        station.isFavorite = true;
        stations.append(station);
    }
    return stations;
}
