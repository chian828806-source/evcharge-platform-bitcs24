#pragma once

#include "models/predictioninfo.h"

#include <QHash>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QString>

class PredictionRepository
{
public:
    QJsonArray list(QSqlDatabase &database, qint64 stationId, const QString &horizon,
                    int limit, QString *errorMessage) const;
    QJsonArray recommendation(QSqlDatabase &database, const QString &horizon,
                              int limit, QString *errorMessage) const;
    QJsonArray warning(QSqlDatabase &database, const QString &horizon,
                       int limit, QString *errorMessage) const;
    QHash<qint64, PredictionInfo> listLatestByHorizon(
        QSqlDatabase &database, const QString &horizon,
        QString *errorMessage) const;
};
