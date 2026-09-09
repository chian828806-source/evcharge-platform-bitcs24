/* 用户与充电站收藏关系的参数化SQL访问。 */
#pragma once

#include "models/stationinfo.h"

#include <QList>
#include <QSet>
#include <QSqlDatabase>
#include <QString>

class FavoriteRepository
{
public:
    bool isFavorite(QSqlDatabase &database, qint64 userId, qint64 stationId,
                    bool *favorite, QString *errorMessage) const;
    bool insert(QSqlDatabase &database, qint64 userId, qint64 stationId,
                const QString &createdAt, QString *errorMessage) const;
    bool remove(QSqlDatabase &database, qint64 userId, qint64 stationId,
                QString *errorMessage) const;
    QSet<qint64> stationIds(QSqlDatabase &database, qint64 userId,
                            QString *errorMessage) const;
    QList<StationInfo> listForUser(QSqlDatabase &database, qint64 userId,
                                   QString *errorMessage) const;
};
