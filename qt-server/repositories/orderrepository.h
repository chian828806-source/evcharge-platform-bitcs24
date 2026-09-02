/*
 * 功能：集中处理订单创建、活动订单查询和电桩预约更新SQL。
 * 边界：不决定业务错误码或用户权限，所有写入事务由Service持有。
 */
#pragma once

#include "models/chargingorder.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

class QSqlQuery;

class OrderRepository
{
public:
    std::optional<ChargingOrderInfo> findActiveByUser(QSqlDatabase &database,
                                                       qint64 userId,
                                                       QString *errorMessage) const;
    std::optional<ChargingOrderInfo> findByIdForUser(QSqlDatabase &database,
                                                      qint64 orderId, qint64 userId,
                                                      QString *errorMessage) const;
    std::optional<OrderCreateTarget> findCreateTarget(QSqlDatabase &database,
                                                      qint64 pileId,
                                                      QString *errorMessage) const;
    bool insertCreated(QSqlDatabase &database, const ChargingOrderInfo &order,
                       qint64 *orderId, QString *errorMessage) const;
    bool reservePile(QSqlDatabase &database, qint64 pileId, qint64 orderId,
                     const QString &now, bool *reserved,
                     QString *errorMessage) const;
    bool startOrder(QSqlDatabase &database, qint64 orderId, const QString &now,
                    bool *started, QString *errorMessage) const;
    bool stopOrder(QSqlDatabase &database, qint64 orderId, const QString &now,
                   int chargeMinutes, double energyKwh, qint64 amountFen,
                   bool *stopped, QString *errorMessage) const;
    bool cancelOrder(QSqlDatabase &database, qint64 orderId, const QString &now,
                     const QString &reason, bool *cancelled,
                     QString *errorMessage) const;
    bool completeOrder(QSqlDatabase &database, qint64 orderId, const QString &now,
                       bool *completed, QString *errorMessage) const;
    bool startPile(QSqlDatabase &database, qint64 pileId, qint64 orderId,
                   const QString &now, bool *started,
                   QString *errorMessage) const;
    bool releasePile(QSqlDatabase &database, qint64 pileId, qint64 orderId,
                     const QString &expectedStatus, const QString &now,
                     bool *released, QString *errorMessage) const;
    bool addPileStatistics(QSqlDatabase &database, qint64 pileId,
                           int chargeMinutes, double energyKwh,
                           const QString &now, QString *errorMessage) const;

private:
    static ChargingOrderInfo mapOrder(const QSqlQuery &query);
};
