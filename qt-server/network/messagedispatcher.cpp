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
    m_routes.insert(messageType, {access, std::move(handler)});
}

ResponseMessage MessageDispatcher::dispatch(const RequestMessage &request) const
{
    // 第一层只允许公共文档登记的30种TCP消息进入系统。
    if (!MessageTypes::tcpTypes().contains(request.type)) {
        return ResponseMessage::error(
            request.requestId, ErrorCodes::InvalidSocketMessage,
            QStringLiteral("unknown message type"));
    }

    // 消息合法但业务模块尚未接入时，明确返回内部处理未就绪。
    const auto route = m_routes.constFind(request.type);
    if (route == m_routes.cend() || !route->handler) {
        return ResponseMessage::error(
            request.requestId, ErrorCodes::InternalError,
            QStringLiteral("business handler is not registered"));
    }

    // 鉴权成功后context中的principalId可以被Handler信任。
    SessionContext context;
    if (!authorize(request, route->access, &context)) {
        return ResponseMessage::error(
            request.requestId, ErrorCodes::InvalidSession,
            QStringLiteral("invalid session or role"));
    }
    return route->handler(request, context);
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
