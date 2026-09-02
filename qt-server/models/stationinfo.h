/*
 * 功能：定义面向用户端的站点摘要、站点详情和电桩数据。
 * 边界：只保存数据和Socket字段转换，不执行SQL、距离计算或业务校验。
 */
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct ChargingPileInfo
{
    qint64 pileId = 0;
    qint64 stationId = 0;
    QString pileNo;
    QString type;
    double powerKw = 0.0;
    QString status;

    QJsonObject toJson() const
    {
        return {
            {QStringLiteral("pileId"), pileId},
            {QStringLiteral("stationId"), stationId},
            {QStringLiteral("pileNo"), pileNo},
            {QStringLiteral("type"), type},
            {QStringLiteral("powerKw"), powerKw},
            {QStringLiteral("status"), status}
        };
    }
};

struct StationInfo
{
    qint64 stationId = 0;
    QString stationNo;
    QString name;
    QString address;
    QString district;
    double longitude = 0.0;
    double latitude = 0.0;
    qint64 priceFenPerKwh = 0;
    qint64 serviceFeeFenPerKwh = 0;
    int pileCount = 0;
    int availablePileCount = 0;
    // 小于0表示当前场景没有用户坐标，例如站点详情请求。
    double distanceKm = -1.0;
    // 仅推荐接口填充；普通站点列表不返回这些字段。
    bool recommended = false;
    double predictedLoad = -1.0;
    int predictedAvailablePileCount = -1;
    QString recommendationReason;

    QJsonObject toJson() const
    {
        QJsonObject json{
            {QStringLiteral("stationId"), stationId},
            {QStringLiteral("stationNo"), stationNo},
            {QStringLiteral("name"), name},
            {QStringLiteral("address"), address},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("latitude"), latitude},
            {QStringLiteral("priceFenPerKwh"), priceFenPerKwh},
            {QStringLiteral("serviceFeeFenPerKwh"), serviceFeeFenPerKwh},
            {QStringLiteral("pileCount"), pileCount},
            {QStringLiteral("availablePileCount"), availablePileCount}
        };
        json.insert(QStringLiteral("district"), district.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(district));
        if (distanceKm >= 0.0) {
            json.insert(QStringLiteral("distanceKm"), distanceKm);
        }
        if (recommended) {
            json.insert(QStringLiteral("recommended"), true);
            json.insert(QStringLiteral("predictedLoad"), predictedLoad);
            json.insert(QStringLiteral("predictedAvailablePileCount"),
                        predictedAvailablePileCount);
            json.insert(QStringLiteral("recommendationReason"), recommendationReason);
        }
        return json;
    }
};

struct StationDetail
{
    StationInfo station;
    QList<ChargingPileInfo> piles;
};
