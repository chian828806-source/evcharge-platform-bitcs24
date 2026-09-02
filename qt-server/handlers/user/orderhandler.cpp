/*
 * 功能：实现ORDER_ACTIVE_CHECK和ORDER_CREATE的Socket映射。
 */
#include "orderhandler.h"

#include "services/user/orderservice.h"
#include "shared/protocol/errorcodes.h"

namespace {

bool isPositiveInteger(const QJsonValue &value)
{
    const double number = value.toDouble();
    return value.isDouble() && number > 0.0
        && number == static_cast<double>(static_cast<qint64>(number));
}

}

OrderHandler::OrderHandler(OrderService *orderService)
    : m_orderService(orderService)
{
}

ResponseMessage OrderHandler::activeCheck(const RequestMessage &request,
                                          const SessionContext &context)
{
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->activeOrder(context.principalId);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    QJsonObject data{
        {QStringLiteral("hasActiveOrder"), result.value.hasActiveOrder},
        {QStringLiteral("balanceFen"), result.value.balanceFen}
    };
    data.insert(QStringLiteral("order"), result.value.hasActiveOrder
        ? QJsonValue(result.value.order.toJson()) : QJsonValue(QJsonValue::Null));
    return ResponseMessage::success(request.requestId, data);
}

ResponseMessage OrderHandler::create(const RequestMessage &request,
                                     const SessionContext &context)
{
    const QJsonValue pileIdValue = request.payload.value(QStringLiteral("pileId"));
    if (!isPositiveInteger(pileIdValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("pileId must be a positive integer"));
    }
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->create(
        context.principalId, static_cast<qint64>(pileIdValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("order"), result.value.toJson()}
    });
}

ResponseMessage OrderHandler::start(const RequestMessage &request,
                                    const SessionContext &context)
{
    const QJsonValue orderIdValue = request.payload.value(QStringLiteral("orderId"));
    if (!isPositiveInteger(orderIdValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("orderId must be a positive integer"));
    }
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->start(
        context.principalId, static_cast<qint64>(orderIdValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("order"), result.value.toJson()}
    });
}

ResponseMessage OrderHandler::stop(const RequestMessage &request,
                                   const SessionContext &context)
{
    const QJsonValue orderIdValue = request.payload.value(QStringLiteral("orderId"));
    if (!isPositiveInteger(orderIdValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("orderId must be a positive integer"));
    }
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->stop(
        context.principalId, static_cast<qint64>(orderIdValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("order"), result.value.toJson()}
    });
}

ResponseMessage OrderHandler::cancel(const RequestMessage &request,
                                     const SessionContext &context)
{
    const QJsonValue orderIdValue = request.payload.value(QStringLiteral("orderId"));
    if (!isPositiveInteger(orderIdValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("orderId must be a positive integer"));
    }
    QString reason;
    const QJsonValue reasonValue = request.payload.value(QStringLiteral("reason"));
    if (!reasonValue.isUndefined() && !reasonValue.isNull()) {
        if (!reasonValue.isString()) {
            return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                          QStringLiteral("reason must be a string"));
        }
        reason = reasonValue.toString();
    }
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->cancel(
        context.principalId, static_cast<qint64>(orderIdValue.toDouble()), reason);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("order"), result.value.toJson()}
    });
}

ResponseMessage OrderHandler::settle(const RequestMessage &request,
                                     const SessionContext &context)
{
    const QJsonValue orderIdValue = request.payload.value(QStringLiteral("orderId"));
    if (!isPositiveInteger(orderIdValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("orderId must be a positive integer"));
    }
    if (!m_orderService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("order module is unavailable"));
    }
    const auto result = m_orderService->settle(
        context.principalId, static_cast<qint64>(orderIdValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("order"), result.value.order.toJson()},
        {QStringLiteral("balanceFen"), result.value.balanceFen}
    });
}
