/*
 * 功能：查询每个充电站指定预测窗口的最新预测结果。
 * 边界：不决定推荐规则，不查询Socket或用户会话。
 */
#pragma once

#include "models/predictioninfo.h"

#include <QHash>
#include <QSqlDatabase>
#include <QString>

class PredictionRepository
{
public:
    QHash<qint64, PredictionInfo> listLatestByHorizon(
        QSqlDatabase &database, const QString &horizon,
        QString *errorMessage) const;
};
