/*
 * 功能：处理用户端活动订单查询和创建订单消息。
 * 边界：字段校验和响应映射留在Handler，事务状态机留在OrderService。
 */
#pragma once

#include "network/messagedispatcher.h"

class OrderService;

class OrderHandler
{
public:
    explicit OrderHandler(OrderService *orderService);

    ResponseMessage activeCheck(const RequestMessage &request,
                                const SessionContext &context);
    ResponseMessage create(const RequestMessage &request,
                           const SessionContext &context);
    ResponseMessage start(const RequestMessage &request,
                          const SessionContext &context);
    ResponseMessage stop(const RequestMessage &request,
                         const SessionContext &context);
    ResponseMessage cancel(const RequestMessage &request,
                           const SessionContext &context);
    ResponseMessage settle(const RequestMessage &request,
                           const SessionContext &context);

private:
    OrderService *m_orderService = nullptr;
};
