/*
 * 功能：实现用户端首批Socket消息与Service之间的映射。
 */
#include "userhandler.h"

#include "network/sessionmanager.h"
#include "services/user/userservice.h"
#include "shared/protocol/errorcodes.h"

UserHandler::UserHandler(UserService *userService, SessionManager *sessionManager)
    : m_userService(userService), m_sessionManager(sessionManager)
{
}

ResponseMessage UserHandler::login(const RequestMessage &request,
                                   const SessionContext &)
{
    const QJsonValue phoneValue = request.payload.value(QStringLiteral("phone"));
    if (!phoneValue.isString()) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("phone must be a string"));
    }
    if (!m_userService || !m_sessionManager) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }

    const ServiceResult<UserProfile> result = m_userService->login(phoneValue.toString());
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }

    const QString sessionId = m_sessionManager->createSession(
        result.value.userId, SessionRole::User);
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("user"), result.value.toJson()}
    });
}

ResponseMessage UserHandler::profileGet(const RequestMessage &request,
                                        const SessionContext &context)
{
    if (!m_userService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }

    const ServiceResult<UserProfile> result = m_userService->profile(context.principalId);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("user"), result.value.toJson()}
    });
}

ResponseMessage UserHandler::profileUpdate(const RequestMessage &request,
                                           const SessionContext &context)
{
    const QJsonValue nicknameValue = request.payload.value(QStringLiteral("nickname"));
    if (!nicknameValue.isString()) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("nickname must be a string"));
    }
    if (!m_userService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }

    const ServiceResult<UserProfile> result =
        m_userService->updateNickname(context.principalId, nicknameValue.toString());
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("user"), result.value.toJson()}
    });
}
