#pragma once

#include "common/serviceresult.h"
#include <QJsonArray>
#include <QJsonObject>

class DatabaseManager;
class PredictionRepository;

class PredictionService
{
public:
    PredictionService(DatabaseManager *databaseManager, PredictionRepository *repository);
    ServiceResult<QJsonArray> list(qint64 stationId, const QString &horizon, int limit) const;
    ServiceResult<QJsonArray> warning(const QString &horizon, int limit) const;
    ServiceResult<QJsonObject> importBatch(const QJsonObject &document) const;
private:
    DatabaseManager *m_databaseManager;
    PredictionRepository *m_repository;
};
