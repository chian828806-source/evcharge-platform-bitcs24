/*
 * 功能：实现活动订单检查和创建预约订单的事务规则。
 * 边界：不直接解析Socket请求；后续开始、停止、结算将继续扩展此服务。
 */
#pragma once

#include "common/serviceresult.h"
#include "models/chargingorder.h"

class DatabaseManager;
class OrderRepository;

struct OrderListResult
{
    QList<ChargingOrderInfo> items;
    int page = 1;
    int pageSize = 20;
    qint64 total = 0;
};
class QDateTime;
class QSqlDatabase;
class UserRepository;

struct ActiveOrderResult
{
    bool hasActiveOrder = false;
    ChargingOrderInfo order;
    qint64 balanceFen = 0;
};

struct SettlementResult
{
    ChargingOrderInfo order;
    qint64 balanceFen = 0;
};

class OrderService
{
public:
    OrderService(DatabaseManager *databaseManager, UserRepository *userRepository,
                 OrderRepository *orderRepository);

    ServiceResult<ActiveOrderResult> activeOrder(qint64 userId);
    ServiceResult<OrderListResult> list(qint64 userId, int page, int pageSize,
                                        const QString &status);
    ServiceResult<ChargingOrderInfo> create(qint64 userId, qint64 pileId);
    ServiceResult<ChargingOrderInfo> start(qint64 userId, qint64 orderId);
    ServiceResult<ChargingOrderInfo> stop(qint64 userId, qint64 orderId);
    ServiceResult<ChargingOrderInfo> cancel(qint64 userId, qint64 orderId,
                                            const QString &reason);
    ServiceResult<SettlementResult> settle(qint64 userId, qint64 orderId);

private:
    bool openDatabase(QSqlDatabase *database, QString *errorMessage) const;
    static ChargingOrderInfo withCurrentProgress(const ChargingOrderInfo &order,
                                                 const QDateTime &now);

    DatabaseManager *m_databaseManager = nullptr;
    UserRepository *m_userRepository = nullptr;
    OrderRepository *m_orderRepository = nullptr;
};
