/*
 * 功能：实现用户端首批Socket消息与Service之间的映射。
 */
#include "userhandler.h"

#include "network/sessionmanager.h"
#include "services/user/userservice.h"
#include "shared/protocol/errorcodes.h"

namespace {

bool isPositiveInteger(const QJsonValue &value)
{
    const double number = value.toDouble();
    return value.isDouble() && number > 0.0
        && number == static_cast<double>(static_cast<qint64>(number));
}

}

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

ResponseMessage UserHandler::avatarUpload(const RequestMessage &request,
                                          const SessionContext &context)
{
    const QJsonValue fileNameValue = request.payload.value(QStringLiteral("fileName"));
    const QJsonValue mimeTypeValue = request.payload.value(QStringLiteral("mimeType"));
    const QJsonValue contentValue = request.payload.value(QStringLiteral("contentBase64"));
    if (!fileNameValue.isString() || !mimeTypeValue.isString() || !contentValue.isString()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("invalid avatar upload payload"));
    }
    if (!m_userService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }
    const auto result = m_userService->uploadAvatar(
        context.principalId, fileNameValue.toString(), mimeTypeValue.toString(),
        contentValue.toString());
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("avatarPath"), result.value.avatarPath},
        {QStringLiteral("user"), result.value.toJson()}
    });
}

ResponseMessage UserHandler::avatarGet(const RequestMessage &request,
                                       const SessionContext &context)
{
    if (!m_userService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }
    const auto result = m_userService->avatarContent(context.principalId);
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    return ResponseMessage::success(request.requestId, result.value.toJson());
}

ResponseMessage UserHandler::recharge(const RequestMessage &request,
                                      const SessionContext &context)
{
    const QJsonValue amountValue = request.payload.value(QStringLiteral("amountFen"));
    if (!isPositiveInteger(amountValue)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("amountFen must be a positive integer"));
    }
    if (!m_userService) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("user module is unavailable"));
    }
    const QString cacheKey = QString::number(context.principalId)
        + QLatin1Char(':') + request.requestId;
    const auto cached = m_rechargeResponses.constFind(cacheKey);
    if (cached != m_rechargeResponses.cend()) {
        return *cached;
    }
    const auto result = m_userService->recharge(
        context.principalId, static_cast<qint64>(amountValue.toDouble()));
    if (!result.ok) {
        return ResponseMessage::error(request.requestId, result.code, result.message);
    }
    const ResponseMessage response = ResponseMessage::success(request.requestId,
                                                                result.value.toJson());
    m_rechargeResponses.insert(cacheKey, response);
    return response;
}
