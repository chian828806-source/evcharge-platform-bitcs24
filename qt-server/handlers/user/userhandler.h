/*
 * 功能：处理USER_LOGIN、USER_PROFILE_GET和USER_PROFILE_UPDATE消息。
 * 边界：只校验payload、调用UserService并映射统一ResponseMessage。
 */
#pragma once

#include "network/messagedispatcher.h"

class SessionManager;
class UserService;

class UserHandler
{
public:
    UserHandler(UserService *userService, SessionManager *sessionManager);

    ResponseMessage login(const RequestMessage &request,
                          const SessionContext &context);
    ResponseMessage profileGet(const RequestMessage &request,
                               const SessionContext &context);
    ResponseMessage profileUpdate(const RequestMessage &request,
                                  const SessionContext &context);

private:
    UserService *m_userService = nullptr;
    SessionManager *m_sessionManager = nullptr;
};
