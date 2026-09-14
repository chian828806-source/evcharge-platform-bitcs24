#include "favoriteservice.h"

#include "database/databasemanager.h"
#include "repositories/favoriterepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QtMath>

namespace {
double distanceKm(double fromLongitude, double fromLatitude,
                  double toLongitude, double toLatitude)
{
    constexpr double radius = 6371.0088;
    const double latDelta = qDegreesToRadians(toLatitude - fromLatitude);
    const double lonDelta = qDegreesToRadians(toLongitude - fromLongitude);
    const double a = qSin(latDelta / 2) * qSin(latDelta / 2)
        + qCos(qDegreesToRadians(fromLatitude)) * qCos(qDegreesToRadians(toLatitude))
        * qSin(lonDelta / 2) * qSin(lonDelta / 2);
    return qRound(radius * 2 * qAtan2(qSqrt(a), qSqrt(1 - a)) * 100.0) / 100.0;
}
}

FavoriteService::FavoriteService(DatabaseManager *databaseManager,
                                 FavoriteRepository *favoriteRepository,
                                 UserRepository *userRepository,
                                 StationRepository *stationRepository)
    : m_databaseManager(databaseManager), m_favoriteRepository(favoriteRepository),
      m_userRepository(userRepository), m_stationRepository(stationRepository)
{
}

ServiceResult<FavoriteToggleResult> FavoriteService::toggle(qint64 userId, qint64 stationId)
{
    if (userId <= 0 || stationId <= 0 || !m_databaseManager || !m_favoriteRepository
        || !m_userRepository || !m_stationRepository) {
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid favorite request"));
    }
    QSqlDatabase database;
    QString error;
    if (!m_databaseManager->database(&database, &error)) {
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &error);
    if (!user.has_value()) {
        return ServiceResult<FavoriteToggleResult>::failure(
            error.isEmpty() ? ErrorCodes::InvalidSession : ErrorCodes::DatabaseError,
            error.isEmpty() ? QStringLiteral("user no longer exists")
                            : QStringLiteral("query user failed"));
    }
    if (user->status != QStringLiteral("NORMAL")) {
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
    }
    if (!m_stationRepository->findById(database, stationId, &error).has_value()) {
        return ServiceResult<FavoriteToggleResult>::failure(
            error.isEmpty() ? ErrorCodes::StationNotFound : ErrorCodes::DatabaseError,
            error.isEmpty() ? QStringLiteral("station not found")
                            : QStringLiteral("query station failed"));
    }
    if (!database.transaction()) {
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("cannot start transaction"));
    }
    bool existing = false;
    if (!m_favoriteRepository->isFavorite(database, userId, stationId, &existing, &error)) {
        database.rollback();
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query favorite failed"));
    }
    const bool ok = existing
        ? m_favoriteRepository->remove(database, userId, stationId, &error)
        : m_favoriteRepository->insert(database, userId, stationId,
              QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), &error);
    if (!ok || !database.commit()) {
        database.rollback();
        return ServiceResult<FavoriteToggleResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("update favorite failed"));
    }
    return ServiceResult<FavoriteToggleResult>::success({stationId, !existing});
}

ServiceResult<QList<StationInfo>> FavoriteService::list(qint64 userId, bool hasLocation,
                                                        double longitude, double latitude)
{
    if (userId <= 0 || !m_databaseManager || !m_favoriteRepository || !m_userRepository) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid favorite request"));
    }
    if (hasLocation && (longitude < -180 || longitude > 180 || latitude < -90 || latitude > 90)) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid coordinates"));
    }
    QSqlDatabase database;
    QString error;
    if (!m_databaseManager->database(&database, &error)) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &error);
    if (!user.has_value()) {
        return ServiceResult<QList<StationInfo>>::failure(
            error.isEmpty() ? ErrorCodes::InvalidSession : ErrorCodes::DatabaseError,
            error.isEmpty() ? QStringLiteral("user no longer exists")
                            : QStringLiteral("query user failed"));
    }
    if (user->status != QStringLiteral("NORMAL")) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
    }
    QList<StationInfo> stations = m_favoriteRepository->listForUser(database, userId, &error);
    if (!error.isEmpty()) {
        return ServiceResult<QList<StationInfo>>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query favorites failed"));
    }
    if (hasLocation) {
        for (StationInfo &station : stations) {
            station.distanceKm = distanceKm(longitude, latitude,
                                            station.longitude, station.latitude);
        }
    }
    return ServiceResult<QList<StationInfo>>::success(stations);
}
