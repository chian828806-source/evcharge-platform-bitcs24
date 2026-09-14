/*
 * 功能：按消息type把请求路由到业务Handler，并在调用前完成Session鉴权。
 * 边界：Dispatcher不解析业务字段、不写SQL，也不决定订单状态。
 */
#pragma once

#include "sessionmanager.h"
#include "shared/protocol/protocolmessage.h"

#include <QHash>
#include <functional>

// MessageDispatcher是网络层与业务Service之间的唯一分发入口。
class MessageDispatcher
{
public:
    // 注册路由时显式声明访问角色，避免业务Handler重复写鉴权代码。
    enum class Access {
        Public,
        User,
        Admin,
        AnyAuthenticated
    };

    // Handler接收已解析请求和可信Session身份，返回标准响应。
    using Handler = std::function<ResponseMessage(const RequestMessage &,
                                                   const SessionContext &)>;
    // 外部 API 等耗时任务完成后通过回调返回；ClientSession 会按原请求连接写回响应。
    using ResponseCallback = std::function<void(const ResponseMessage &)>;
    using AsyncHandler = std::function<void(const RequestMessage &, const SessionContext &,
                                            ResponseCallback)>;

    // 依赖由外部注入，便于测试和后续服务端程序装配。
    explicit MessageDispatcher(SessionManager *sessionManager);

    // 安装或替换一个消息路由；实际业务模块在程序装配阶段调用。
    void registerHandler(const QString &messageType, Access access,
                         Handler handler);
    void registerAsyncHandler(const QString &messageType, Access access,
                              AsyncHandler handler);
    // 完成消息登记检查、路由查找、鉴权和Handler调用。
    ResponseMessage dispatch(const RequestMessage &request) const;
    // 网络会话使用此入口，既支持立即响应，也支持外部服务完成后的异步响应。
    void dispatchAsync(const RequestMessage &request, ResponseCallback callback) const;

private:
    // 路由表把访问策略与业务回调绑定在一起。
    struct Route {
        Access access = Access::AnyAuthenticated;
        Handler handler;
        AsyncHandler asyncHandler;
    };

    // 只验证公共Session和角色，不检查冻结、订单归属等业务条件。
    bool authorize(const RequestMessage &request, Access access,
                   SessionContext *context) const;
    ResponseMessage preflight(const RequestMessage &request, Route *route,
                              SessionContext *context) const;

    SessionManager *m_sessionManager = nullptr;
    QHash<QString, Route> m_routes;
};
