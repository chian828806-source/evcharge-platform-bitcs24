/*
 * 功能：实现订单创建事务与活动订单查询。
 */
#include "orderservice.h"

#include "database/databasemanager.h"
#include "repositories/orderrepository.h"
#include "repositories/userrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QSet>
#include <QSqlDatabase>
#include <QUuid>
#include <QtMath>

OrderService::OrderService(DatabaseManager *databaseManager,
                           UserRepository *userRepository,
                           OrderRepository *orderRepository)
    : m_databaseManager(databaseManager), m_userRepository(userRepository),
      m_orderRepository(orderRepository)
{
}

ServiceResult<ActiveOrderResult> OrderService::activeOrder(qint64 userId)
{
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<ActiveOrderResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value()) {
        return ServiceResult<ActiveOrderResult>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession
                                    : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("user no longer exists")
                                    : QStringLiteral("query user failed"));
    }
    const auto order = m_orderRepository->findActiveByUser(database, userId, &databaseError);
    if (!databaseError.isEmpty()) {
        return ServiceResult<ActiveOrderResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query active order failed"));
    }
    ActiveOrderResult result;
    result.hasActiveOrder = order.has_value();
    result.balanceFen = user->balanceFen;
    if (order.has_value()) {
        result.order = withCurrentProgress(*order, QDateTime::currentDateTime());
    }
    return ServiceResult<ActiveOrderResult>::success(result);
}

ServiceResult<OrderListResult> OrderService::list(qint64 userId, int page,
                                                   int pageSize,
                                                   const QString &status)
{
    static const QSet<QString> allowedStatuses{
        QStringLiteral("CREATED"), QStringLiteral("CHARGING"),
        QStringLiteral("PENDING_PAYMENT"), QStringLiteral("COMPLETED"),
        QStringLiteral("CANCELLED")
    };
    const QString normalizedStatus = status.trimmed().toUpper();
    if (page < 1 || pageSize < 1 || pageSize > 50
        || (!normalizedStatus.isEmpty() && !allowedStatuses.contains(normalizedStatus))) {
        return ServiceResult<OrderListResult>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid order list query"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<OrderListResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const qint64 total = m_orderRepository->countByUser(
        database, userId, normalizedStatus, &databaseError);
    if (total < 0) {
        return ServiceResult<OrderListResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("count orders failed"));
    }
    const QList<ChargingOrderInfo> orders = m_orderRepository->listByUser(
        database, userId, normalizedStatus, pageSize, (page - 1) * pageSize,
        &databaseError);
    if (!databaseError.isEmpty()) {
        return ServiceResult<OrderListResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query order list failed"));
    }
    return ServiceResult<OrderListResult>::success({orders, page, pageSize, total});
}

ServiceResult<ChargingOrderInfo> OrderService::start(qint64 userId, qint64 orderId)
{
    if (orderId <= 0) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid orderId"));
    }
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession : ErrorCodes::DatabaseError,
            QStringLiteral("user is unavailable"));
    }
    if (user->status != QStringLiteral("NORMAL")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
    }
    const auto order = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                            &databaseError);
    if (!order.has_value()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidOrderState
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order is unavailable"));
    }
    if (order->status != QStringLiteral("CREATED")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::InvalidOrderState, QStringLiteral("order cannot be started"));
    }
    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    bool pileStarted = false;
    bool orderStarted = false;
    if (!m_orderRepository->startPile(database, order->pileId, orderId, now,
                                      &pileStarted, &databaseError)) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("start pile failed"));
    }
    if (!pileStarted) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::PileUnavailable, QStringLiteral("pile reservation is invalid"));
    }
    if (!m_orderRepository->startOrder(database, orderId, now, &orderStarted,
                                       &databaseError) || !orderStarted) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidOrderState
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order cannot be started"));
    }
    const auto savedOrder = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                                 &databaseError);
    if (!savedOrder.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read started order failed"));
    }
    return ServiceResult<ChargingOrderInfo>::success(
        withCurrentProgress(*savedOrder, QDateTime::currentDateTime()));
}

ServiceResult<ChargingOrderInfo> OrderService::stop(qint64 userId, qint64 orderId)
{
    if (orderId <= 0) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid orderId"));
    }
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto order = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                            &databaseError);
    if (!order.has_value() || order->status != QStringLiteral("CHARGING")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidOrderState
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order cannot be stopped"));
    }
    const QDateTime nowDateTime = QDateTime::currentDateTime();
    const ChargingOrderInfo progress = withCurrentProgress(*order, nowDateTime);
    const QString now = nowDateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    bool orderStopped = false;
    bool pileReleased = false;
    if (!m_orderRepository->stopOrder(database, orderId, now, progress.chargeMinutes,
                                      progress.energyKwh, progress.amountFen,
                                      &orderStopped, &databaseError)
        || !orderStopped) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidOrderState
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order cannot be stopped"));
    }
    if (!m_orderRepository->releasePile(database, order->pileId, orderId,
                                        QStringLiteral("CHARGING"), now,
                                        &pileReleased, &databaseError) || !pileReleased) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::PileUnavailable
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("pile cannot be released"));
    }
    const auto savedOrder = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                                 &databaseError);
    if (!savedOrder.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read stopped order failed"));
    }
    return ServiceResult<ChargingOrderInfo>::success(
        withCurrentProgress(*savedOrder, nowDateTime));
}

ServiceResult<ChargingOrderInfo> OrderService::cancel(qint64 userId, qint64 orderId,
                                                       const QString &reason)
{
    if (orderId <= 0) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid orderId"));
    }
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto order = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                            &databaseError);
    if (!order.has_value() || order->status != QStringLiteral("CREATED")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::OrderCannotCancel
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order cannot be cancelled"));
    }
    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    bool cancelled = false;
    bool released = false;
    if (!m_orderRepository->cancelOrder(database, orderId, now, reason.trimmed(),
                                        &cancelled, &databaseError) || !cancelled
        || !m_orderRepository->releasePile(database, order->pileId, orderId,
                                           QStringLiteral("RESERVED"), now,
                                           &released, &databaseError) || !released) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::OrderCannotCancel
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order cancellation failed"));
    }
    const auto savedOrder = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                                 &databaseError);
    if (!savedOrder.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read cancelled order failed"));
    }
    return ServiceResult<ChargingOrderInfo>::success(*savedOrder);
}

ServiceResult<SettlementResult> OrderService::settle(qint64 userId, qint64 orderId)
{
    if (orderId <= 0) {
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid orderId"));
    }
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &databaseError);
    const auto order = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                            &databaseError);
    if (!user.has_value() || !order.has_value()) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidOrderState
                                    : ErrorCodes::DatabaseError,
            QStringLiteral("order or user is unavailable"));
    }
    if (order->status == QStringLiteral("COMPLETED")) {
        database.commit();
        return ServiceResult<SettlementResult>::success({
            withCurrentProgress(*order, QDateTime::currentDateTime()), user->balanceFen});
    }
    if (order->status != QStringLiteral("PENDING_PAYMENT")) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::InvalidOrderState, QStringLiteral("order cannot be settled"));
    }
    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    bool deducted = false;
    if (!m_userRepository->decreaseBalance(database, userId, order->amountFen, now,
                                           &deducted, &databaseError)) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("deduct balance failed"));
    }
    if (!deducted) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::InsufficientBalance, QStringLiteral("insufficient balance"));
    }
    bool completed = false;
    if (!m_orderRepository->completeOrder(database, orderId, now, &completed,
                                          &databaseError) || !completed
        || !m_orderRepository->addPileStatistics(database, order->pileId,
                                                 order->chargeMinutes,
                                                 order->energyKwh, now,
                                                 &databaseError)) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("complete order failed"));
    }
    const auto savedOrder = m_orderRepository->findByIdForUser(database, orderId, userId,
                                                                 &databaseError);
    const auto savedUser = m_userRepository->findById(database, userId, &databaseError);
    if (!savedOrder.has_value() || !savedUser.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<SettlementResult>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read settled order failed"));
    }
    return ServiceResult<SettlementResult>::success({
        withCurrentProgress(*savedOrder, QDateTime::currentDateTime()), savedUser->balanceFen});
}

ServiceResult<ChargingOrderInfo> OrderService::create(qint64 userId, qint64 pileId)
{
    if (pileId <= 0) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid pileId"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    if (!database.transaction()) {
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("cannot start transaction"));
    }

    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession
                                    : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("user no longer exists")
                                    : QStringLiteral("query user failed"));
    }
    if (user->status != QStringLiteral("NORMAL")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
    }

    const auto existing = m_orderRepository->findActiveByUser(database, userId,
                                                               &databaseError);
    if (!databaseError.isEmpty()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query active order failed"));
    }
    if (existing.has_value()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::ActiveOrderExists, QStringLiteral("active order already exists"));
    }

    const auto target = m_orderRepository->findCreateTarget(database, pileId,
                                                             &databaseError);
    if (!target.has_value()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            databaseError.isEmpty() ? ErrorCodes::PileNotFound
                                    : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("pile not found")
                                    : QStringLiteral("query pile failed"));
    }
    if (target->stationStatus != QStringLiteral("NORMAL")
        || target->pileStatus != QStringLiteral("AVAILABLE")) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::PileUnavailable, QStringLiteral("pile is unavailable"));
    }

    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    ChargingOrderInfo order;
    order.orderNo = QStringLiteral("O-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz"))
        + QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    order.userId = userId;
    order.stationId = target->stationId;
    order.pileId = target->pileId;
    order.status = QStringLiteral("CREATED");
    order.priceFenPerKwh = target->priceFenPerKwh;
    order.serviceFeeFenPerKwh = target->serviceFeeFenPerKwh;
    order.createdAt = now;

    qint64 orderId = 0;
    if (!m_orderRepository->insertCreated(database, order, &orderId, &databaseError)) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("create order failed"));
    }
    bool reserved = false;
    if (!m_orderRepository->reservePile(database, pileId, orderId, now, &reserved,
                                        &databaseError)) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("reserve pile failed"));
    }
    if (!reserved) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::PileUnavailable, QStringLiteral("pile was reserved by another user"));
    }

    const auto savedOrder = m_orderRepository->findByIdForUser(database, orderId,
                                                                 userId, &databaseError);
    if (!savedOrder.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<ChargingOrderInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read created order failed"));
    }
    return ServiceResult<ChargingOrderInfo>::success(*savedOrder);
}

bool OrderService::openDatabase(QSqlDatabase *database, QString *errorMessage) const
{
    return m_databaseManager && m_userRepository && m_orderRepository
        && m_databaseManager->database(database, errorMessage);
}

ChargingOrderInfo OrderService::withCurrentProgress(const ChargingOrderInfo &order,
                                                     const QDateTime &now)
{
    const QDateTime start = QDateTime::fromString(
        order.startAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!start.isValid()) {
        return order;
    }
    QDateTime end = now;
    if (order.status != QStringLiteral("CHARGING")) {
        end = QDateTime::fromString(order.endAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!end.isValid()) {
            return order;
        }
    }
    ChargingOrderInfo result = order;
    const qint64 elapsedSeconds = qMax<qint64>(0, start.secsTo(end));
    result.chargeSeconds = elapsedSeconds;
    if (order.status == QStringLiteral("CHARGING")) {
        result.chargeMinutes = static_cast<int>(elapsedSeconds / 60);
        result.energyKwh = order.powerKw * static_cast<double>(elapsedSeconds) / 3600.0;
        result.amountFen = qRound64(result.energyKwh
            * static_cast<double>(order.priceFenPerKwh + order.serviceFeeFenPerKwh));
    }
    return result;
}
