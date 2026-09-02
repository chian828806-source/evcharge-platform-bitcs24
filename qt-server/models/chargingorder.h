/*
 * 功能：定义Socket返回的订单数据以及创建订单时所需的电桩快照。
 * 边界：模型不执行状态转换、计费或SQL操作。
 */
#pragma once

#include <QJsonObject>
#include <QString>

struct ChargingOrderInfo
{
    qint64 orderId = 0;
    QString orderNo;
    qint64 userId = 0;
    qint64 stationId = 0;
    QString stationName;
    qint64 pileId = 0;
    QString pileNo;
    double powerKw = 0.0;
    QString status;
    qint64 priceFenPerKwh = 0;
    qint64 serviceFeeFenPerKwh = 0;
    QString startAt;
    QString endAt;
    int chargeMinutes = 0;
    double energyKwh = 0.0;
    qint64 amountFen = 0;
    QString createdAt;

    QJsonObject toJson() const
    {
        QJsonObject json{
            {QStringLiteral("orderId"), orderId},
            {QStringLiteral("orderNo"), orderNo},
            {QStringLiteral("userId"), userId},
            {QStringLiteral("stationId"), stationId},
            {QStringLiteral("pileId"), pileId},
            {QStringLiteral("powerKw"), powerKw},
            {QStringLiteral("status"), status},
            {QStringLiteral("priceFenPerKwh"), priceFenPerKwh},
            {QStringLiteral("serviceFeeFenPerKwh"), serviceFeeFenPerKwh},
            {QStringLiteral("chargeMinutes"), chargeMinutes},
            {QStringLiteral("energyKwh"), energyKwh},
            {QStringLiteral("amountFen"), amountFen},
            {QStringLiteral("createdAt"), createdAt}
        };
        json.insert(QStringLiteral("stationName"), stationName.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(stationName));
        json.insert(QStringLiteral("pileNo"), pileNo.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(pileNo));
        json.insert(QStringLiteral("startAt"), startAt.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(startAt));
        json.insert(QStringLiteral("endAt"), endAt.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(endAt));
        return json;
    }
};

struct OrderCreateTarget
{
    qint64 pileId = 0;
    qint64 stationId = 0;
    QString pileStatus;
    QString stationStatus;
    qint64 priceFenPerKwh = 0;
    qint64 serviceFeeFenPerKwh = 0;
};
