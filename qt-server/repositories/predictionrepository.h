#pragma once

#include <QJsonArray>
#include <QSqlDatabase>
#include <QJsonObject>

class PredictionRepository
{
public:
    QJsonArray list(QSqlDatabase &database, qint64 stationId, const QString &horizon,
                    int limit, QString *errorMessage) const;
    QJsonArray recommendation(QSqlDatabase &database, const QString &horizon,
                              int limit, QString *errorMessage) const;
    QJsonArray warning(QSqlDatabase &database, const QString &horizon,
                              int limit, QString *errorMessage) const;
    QJsonObject importBatch(QSqlDatabase &database, const QJsonObject &document,
                            QString *errorMessage) const;
};
