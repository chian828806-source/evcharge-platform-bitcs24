/*
 * 功能：集中注册用户模块消息，避免main.cpp散落具体业务路由。
 */
#pragma once

class MessageDispatcher;
class UserHandler;

void registerUserHandlers(MessageDispatcher *dispatcher, UserHandler *userHandler);
