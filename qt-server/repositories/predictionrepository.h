/*
 * 功能：提供预测推荐和预测列表所需的数据查询。
 * 边界：只执行 SQL；不同调用方可选择领域模型或 JSON 行结果。
 */
#pragma once

#include "models/predictioninfo.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

class PredictionRepository
{
public:
    // 供用户端推荐服务按站点读取指定时间窗口的最新预测。
    QHash<qint64, PredictionInfo> listLatestByHorizon(
        QSqlDatabase &database, const QString &horizon,
        QString *errorMessage) const;

    // 供管理端/预测模块直接返回可展示的预测记录。
    QJsonArray list(QSqlDatabase &database, qint64 stationId, const QString &horizon,
                    int limit, QString *errorMessage) const;
    QJsonArray recommendation(QSqlDatabase &database, const QString &horizon,
                              int limit, QString *errorMessage) const;
    QJsonArray warning(QSqlDatabase &database, const QString &horizon,
                       int limit, QString *errorMessage) const;
    QJsonObject importBatch(QSqlDatabase &database, const QJsonObject &document,
                            QString *errorMessage) const;
};
