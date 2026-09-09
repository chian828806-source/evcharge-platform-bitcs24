#pragma once

#include "models/chargingorder.h"

#include <QDateTime>
#include <QtMath>

inline ChargingOrderInfo chargingProgressAt(const ChargingOrderInfo &order, const QDateTime &now)
{
    const QDateTime start = QDateTime::fromString(order.startAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!start.isValid()) return order;
    ChargingOrderInfo result = order;
    const qint64 elapsedSeconds = qMax<qint64>(0, start.secsTo(now));
    result.chargeSeconds = elapsedSeconds;
    result.chargeMinutes = static_cast<int>(elapsedSeconds / 60);
    result.energyKwh = order.powerKw * static_cast<double>(elapsedSeconds) / 3600.0;
    result.amountFen = qRound64(result.energyKwh * static_cast<double>(
        order.priceFenPerKwh + order.serviceFeeFenPerKwh));
    return result;
}
