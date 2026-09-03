/*
 * 功能：实现站点查询、Haversine距离计算和用户可见性规则。
 */
#include "stationservice.h"

#include "database/databasemanager.h"
#include "repositories/stationrepository.h"
#include "repositories/predictionrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QSqlDatabase>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <utility>

StationService::StationService(DatabaseManager *databaseManager,
                               StationRepository *stationRepository,
                               PredictionRepository *predictionRepository,
                               MapAdapter *mapAdapter)
    : m_databaseManager(databaseManager), m_stationRepository(stationRepository),
      m_predictionRepository(predictionRepository), m_mapAdapter(mapAdapter)
{
}

void StationService::geocode(const QString &district, const QString &address,
                             MapAdapter::Callback callback)
{
    if (address.trimmed().isEmpty()) {
        callback(ServiceResult<MapGeocodeResult>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("address is required")));
        return;
    }
    if (!m_mapAdapter) {
        callback(ServiceResult<MapGeocodeResult>::failure(
            ErrorCodes::InternalError, QStringLiteral("map module is unavailable")));
        return;
    }
    m_mapAdapter->geocode(district, address, std::move(callback));
}

void StationService::planRoute(double originLongitude, double originLatitude,
                               double destinationLongitude, double destinationLatitude,
                               const QString &mode, MapAdapter::RoutePlanCallback callback)
{
    if (!m_mapAdapter) {
        callback(ServiceResult<MapRoutePlanResult>::failure(
            ErrorCodes::InternalError, QStringLiteral("map module is unavailable")));
        return;
    }
    m_mapAdapter->planRoute(originLongitude, originLatitude,
                            destinationLongitude, destinationLatitude,
                            mode, std::move(callback));
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

ServiceResult<QList<StationInfo>> StationService::recommendations(
    double longitude, double latitude, int limit, const QString &horizon)
{
    static const QSet<QString> allowedHorizons{
        QStringLiteral("1h"), QStringLiteral("6h"), QStringLiteral("24h")
    };
    const QString normalizedHorizon = horizon.trimmed().toLower();
    if (!m_predictionRepository) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::InternalError, QStringLiteral("prediction module is unavailable"));
    }
    if (!allowedHorizons.contains(normalizedHorizon) || limit < 1 || limit > 20) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid recommendation query"));
    }
    const auto nearbyResult = listNearby(longitude, latitude, QString(), 50);
    if (!nearbyResult.ok) {
        return nearbyResult;
    }
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const QHash<qint64, PredictionInfo> predictions =
        m_predictionRepository->listLatestByHorizon(database, normalizedHorizon, &databaseError);
    if (!databaseError.isEmpty()) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query predictions failed"));
    }

    QList<StationInfo> recommended;
    for (StationInfo station : nearbyResult.value) {
        const auto prediction = predictions.constFind(station.stationId);
        if (prediction == predictions.cend() || station.availablePileCount <= 0
            || prediction->predictedAvailablePileCount <= 0) {
            continue;
        }
        station.recommended = true;
        station.predictedLoad = prediction->predictedLoad;
        station.predictedAvailablePileCount = prediction->predictedAvailablePileCount;
        station.recommendationReason = QStringLiteral("预测低负荷，预计可用%1个电桩")
            .arg(prediction->predictedAvailablePileCount);
        recommended.append(station);
    }
    std::sort(recommended.begin(), recommended.end(), [](const StationInfo &left,
                                                          const StationInfo &right) {
        if (!qFuzzyCompare(left.predictedLoad + 1.0, right.predictedLoad + 1.0)) {
            return left.predictedLoad < right.predictedLoad;
        }
        if (!qFuzzyCompare(left.distanceKm + 1.0, right.distanceKm + 1.0)) {
            return left.distanceKm < right.distanceKm;
        }
        return left.predictedAvailablePileCount > right.predictedAvailablePileCount;
    });
    if (recommended.size() > limit) {
        recommended = recommended.mid(0, limit);
    }
    return ServiceResult<QList<StationInfo>>::success(std::move(recommended));
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
