/* 功能：将订单查询和创建路由注册为User权限。 */
#include "registerorderhandlers.h"

#include "network/messagedispatcher.h"
#include "orderhandler.h"
#include "shared/protocol/messagetypes.h"

void registerOrderHandlers(MessageDispatcher *dispatcher, OrderHandler *orderHandler)
{
    if (!dispatcher || !orderHandler) {
        return;
    }
    dispatcher->registerHandler(
        MessageTypes::OrderActiveCheck, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->activeCheck(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::UserOrderList, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->list(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::OrderCreate, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->create(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::OrderStart, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->start(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::OrderStop, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->stop(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::OrderCancel, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->cancel(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::OrderSettle, MessageDispatcher::Access::User,
        [orderHandler](const RequestMessage &request, const SessionContext &context) {
            return orderHandler->settle(request, context);
        });
}
