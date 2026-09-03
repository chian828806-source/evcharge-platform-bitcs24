/*
 * 功能：为Web Dashboard组装只读运营快照。
 * 边界：复用既有Repository和PredictionService；WebSocket通信层不访问SQLite。
 */
#pragma once

#include "common/serviceresult.h"

#include <QJsonObject>
#include <QString>

class DatabaseManager;
class PredictionRepository;
class PredictionService;

class DashboardDataService
{
public:
    explicit DashboardDataService(DatabaseManager *databaseManager);
    ~DashboardDataService();

    ServiceResult<QJsonObject> dataForTopic(const QString &topic) const;

private:
    ServiceResult<QJsonObject> summary() const;
    ServiceResult<QJsonObject> pileStatus() const;
    ServiceResult<QJsonObject> revenueTrend() const;
    ServiceResult<QJsonObject> prediction() const;

    DatabaseManager *m_databaseManager = nullptr;
    PredictionRepository *m_predictionRepository = nullptr;
    PredictionService *m_predictionService = nullptr;
};
