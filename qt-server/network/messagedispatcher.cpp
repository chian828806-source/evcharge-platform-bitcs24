/*
 * 功能：实现消息注册、公共类型检查、Session鉴权和Handler分发。
 * 错误：严格复用docs/03-API.md已有的4401、4003和5002。
 */
#include "messagedispatcher.h"

#include "shared/protocol/errorcodes.h"
#include "shared/protocol/messagetypes.h"

MessageDispatcher::MessageDispatcher(SessionManager *sessionManager)
    : m_sessionManager(sessionManager)
{
    // 保存SessionManager地址；其生命周期由main中的程序装配对象管理。
}

void MessageDispatcher::registerHandler(const QString &messageType,
                                        Access access, Handler handler)
{
    // 同一type再次注册时覆盖旧路由，便于测试或模块替换。
    m_routes.insert(messageType, {access, std::move(handler), {}});
}

void MessageDispatcher::registerAsyncHandler(const QString &messageType,
                                             Access access, AsyncHandler handler)
{
    // 同步和异步实现互斥，避免同一消息被两个处理器重复响应。
    m_routes.insert(messageType, {access, {}, std::move(handler)});
}

ResponseMessage MessageDispatcher::dispatch(const RequestMessage &request) const
{
    Route route;
    SessionContext context;
    const ResponseMessage error = preflight(request, &route, &context);
    if (error.code != ErrorCodes::Success) {
        return error;
    }
    if (!route.handler) {
        return ResponseMessage::error(
            request.requestId, ErrorCodes::InternalError,
            QStringLiteral("async handler requires an active client session"));
    }
    return route.handler(request, context);
}

void MessageDispatcher::dispatchAsync(const RequestMessage &request,
                                      ResponseCallback callback) const
{
    Route route;
    SessionContext context;
    const ResponseMessage error = preflight(request, &route, &context);
    if (error.code != ErrorCodes::Success) {
        callback(error);
        return;
    }
    if (route.asyncHandler) {
        route.asyncHandler(request, context, std::move(callback));
        return;
    }
    callback(route.handler(request, context));
}

ResponseMessage MessageDispatcher::preflight(const RequestMessage &request, Route *route,
                                             SessionContext *context) const
{
    // 第一层只允许公共文档登记的31种TCP消息进入系统。
    if (!MessageTypes::tcpTypes().contains(request.type)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("unknown message type"));
    }
    const auto foundRoute = m_routes.constFind(request.type);
    if (foundRoute == m_routes.cend()
        || (!foundRoute->handler && !foundRoute->asyncHandler)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                      QStringLiteral("business handler is not registered"));
    }
    if (!authorize(request, foundRoute->access, context)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSession,
                                      QStringLiteral("invalid session or role"));
    }
    *route = *foundRoute;
    return ResponseMessage::success(request.requestId, {});
}

bool MessageDispatcher::authorize(const RequestMessage &request, Access access,
                                  SessionContext *context) const
{
    // 登录等公开路由不要求Session，context保持默认值。
    if (access == Access::Public) {
        return true;
    }
    // 缺少管理器、令牌为空或查询失败都属于无效Session。
    if (!m_sessionManager || request.sessionId.isEmpty()
        || !m_sessionManager->findSession(request.sessionId, context)) {
        return false;
    }
    // 任意已登录角色通过；其余路由继续检查精确角色。
    if (access == Access::AnyAuthenticated) {
        return true;
    }
    if (access == Access::User) {
        return context->role == SessionRole::User;
    }
    return context->role == SessionRole::Admin;
}
