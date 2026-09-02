/*
 * 功能：处理USER_LOGIN、USER_PROFILE_GET和USER_PROFILE_UPDATE消息。
 * 边界：只校验payload、调用UserService并映射统一ResponseMessage。
 */
#pragma once

#include "network/messagedispatcher.h"

#include <QHash>

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
    ResponseMessage avatarUpload(const RequestMessage &request,
                                 const SessionContext &context);
    ResponseMessage recharge(const RequestMessage &request,
                             const SessionContext &context);

private:
    UserService *m_userService = nullptr;
    SessionManager *m_sessionManager = nullptr;
    // V1以进程内缓存保证同一用户重复提交同一requestId时不会重复充值。
    QHash<QString, ResponseMessage> m_rechargeResponses;
};
