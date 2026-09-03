/*
 * 功能：定义站点推荐使用的最新预测数据。
 * 边界：不计算推荐排序，不执行SQL。
 */
#pragma once

#include <QString>

struct PredictionInfo
{
    qint64 stationId = 0;
    QString horizon;
    double predictedLoad = 0.0;
    int predictedAvailablePileCount = 0;
    QString predictionTime;
};
