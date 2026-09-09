/* 收藏业务规则：用户状态、站点存在性、事务和距离计算。 */
#pragma once

#include "common/serviceresult.h"
#include "models/stationinfo.h"

#include <QList>

class DatabaseManager;
class FavoriteRepository;
class StationRepository;
class UserRepository;

struct FavoriteToggleResult
{
    qint64 stationId = 0;
    bool isFavorite = false;
};

class FavoriteService
{
public:
    FavoriteService(DatabaseManager *databaseManager, FavoriteRepository *favoriteRepository,
                    UserRepository *userRepository, StationRepository *stationRepository);

    ServiceResult<FavoriteToggleResult> toggle(qint64 userId, qint64 stationId);
    ServiceResult<QList<StationInfo>> list(qint64 userId, bool hasLocation,
                                          double longitude, double latitude);

private:
    DatabaseManager *m_databaseManager = nullptr;
    FavoriteRepository *m_favoriteRepository = nullptr;
    UserRepository *m_userRepository = nullptr;
    StationRepository *m_stationRepository = nullptr;
};
