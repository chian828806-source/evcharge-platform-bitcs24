/*
 * 功能：实现普通用户可见的附近站点和站点详情业务规则。
 * 边界：不解析Socket报文，不拼接SQL；距离排序在此统一保证客户端一致。
 */
#pragma once

#include "common/serviceresult.h"
#include "models/stationinfo.h"

#include <QList>
#include <QString>

class DatabaseManager;
class QSqlDatabase;
class StationRepository;

class StationService
{
public:
    StationService(DatabaseManager *databaseManager,
                   StationRepository *stationRepository);

    ServiceResult<QList<StationInfo>> listNearby(double longitude, double latitude,
                                                 const QString &district, int limit);
    ServiceResult<StationDetail> detail(qint64 stationId);

private:
    bool openDatabase(QSqlDatabase *database, QString *errorMessage) const;
    static bool isValidCoordinate(double longitude, double latitude);
    static double distanceKm(double fromLongitude, double fromLatitude,
                             double toLongitude, double toLatitude);

    DatabaseManager *m_databaseManager = nullptr;
    StationRepository *m_stationRepository = nullptr;
};
