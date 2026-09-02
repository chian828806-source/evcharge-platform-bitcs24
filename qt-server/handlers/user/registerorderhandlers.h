/* 功能：集中注册用户端订单第一阶段路由。 */
#pragma once

class MessageDispatcher;
class OrderHandler;

void registerOrderHandlers(MessageDispatcher *dispatcher, OrderHandler *orderHandler);
