/*
 * 功能：集中处理站点、电桩的只读参数化SQL。
 * 边界：Repository不负责坐标校验、距离排序、Socket响应或用户鉴权。
 */
#pragma once

#include "models/stationinfo.h"

#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

#include <optional>

class QSqlQuery;

class StationRepository
{
public:
    QList<StationInfo> listEnabled(QSqlDatabase &database, const QString &district,
                                   QString *errorMessage) const;
    std::optional<StationInfo> findEnabledById(QSqlDatabase &database,
                                               qint64 stationId,
                                               QString *errorMessage) const;
    QList<ChargingPileInfo> listPiles(QSqlDatabase &database, qint64 stationId,
                                      QString *errorMessage) const;
    QJsonArray listForAdmin(QSqlDatabase &database, QString *errorMessage) const;
    qint64 createForAdmin(QSqlDatabase &database, const QJsonObject &values,
                          const QString &stationNo, const QString &now,
                          QString *errorMessage) const;

private:
    static StationInfo mapStation(const QSqlQuery &query);
    static ChargingPileInfo mapPile(const QSqlQuery &query);
};
