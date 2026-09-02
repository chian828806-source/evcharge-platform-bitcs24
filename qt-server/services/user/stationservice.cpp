/*
 * 功能：实现站点查询、Haversine距离计算和用户可见性规则。
 */
#include "stationservice.h"

#include "database/databasemanager.h"
#include "repositories/stationrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QSqlDatabase>
#include <QtMath>

#include <algorithm>

StationService::StationService(DatabaseManager *databaseManager,
                               StationRepository *stationRepository)
    : m_databaseManager(databaseManager), m_stationRepository(stationRepository)
{
}

ServiceResult<QList<StationInfo>> StationService::listNearby(
    double longitude, double latitude, const QString &district, int limit)
{
    if (!isValidCoordinate(longitude, latitude) || limit < 1 || limit > 50) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::InvalidSocketMessage,
            QStringLiteral("invalid coordinate or limit"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }

    QList<StationInfo> stations = m_stationRepository->listEnabled(
        database, district.trimmed(), &databaseError);
    if (!databaseError.isEmpty()) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query stations failed"));
    }
    for (StationInfo &station : stations) {
        station.distanceKm = distanceKm(longitude, latitude,
                                        station.longitude, station.latitude);
    }
    std::sort(stations.begin(), stations.end(), [](const StationInfo &left,
                                                    const StationInfo &right) {
        return left.distanceKm < right.distanceKm;
    });
    if (stations.size() > limit) {
        stations = stations.mid(0, limit);
    }
    return ServiceResult<QList<StationInfo>>::success(std::move(stations));
}

ServiceResult<StationDetail> StationService::detail(qint64 stationId)
{
    if (stationId <= 0) {
        return ServiceResult<StationDetail>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid stationId"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<StationDetail>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }

    const auto station = m_stationRepository->findEnabledById(
        database, stationId, &databaseError);
    if (!station.has_value()) {
        return ServiceResult<StationDetail>::failure(
            databaseError.isEmpty() ? ErrorCodes::StationNotFound
                                    : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("station not found")
                                    : QStringLiteral("query station failed"));
    }
    const QList<ChargingPileInfo> piles = m_stationRepository->listPiles(
        database, stationId, &databaseError);
    if (!databaseError.isEmpty()) {
        return ServiceResult<StationDetail>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query piles failed"));
    }
    return ServiceResult<StationDetail>::success({*station, piles});
}

bool StationService::openDatabase(QSqlDatabase *database,
                                  QString *errorMessage) const
{
    return m_databaseManager && m_stationRepository
        && m_databaseManager->database(database, errorMessage);
}

bool StationService::isValidCoordinate(double longitude, double latitude)
{
    return longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

double StationService::distanceKm(double fromLongitude, double fromLatitude,
                                  double toLongitude, double toLatitude)
{
    constexpr double earthRadiusKm = 6371.0088;
    const double latitudeDelta = qDegreesToRadians(toLatitude - fromLatitude);
    const double longitudeDelta = qDegreesToRadians(toLongitude - fromLongitude);
    const double fromLatitudeRadians = qDegreesToRadians(fromLatitude);
    const double toLatitudeRadians = qDegreesToRadians(toLatitude);
    const double a = qSin(latitudeDelta / 2.0) * qSin(latitudeDelta / 2.0)
        + qCos(fromLatitudeRadians) * qCos(toLatitudeRadians)
        * qSin(longitudeDelta / 2.0) * qSin(longitudeDelta / 2.0);
    const double angularDistance = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return qRound(earthRadiusKm * angularDistance * 100.0) / 100.0;
}
