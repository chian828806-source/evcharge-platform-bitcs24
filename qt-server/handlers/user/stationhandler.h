/*
 * 功能：校验站点查询payload并把StationService结果映射为公共Socket响应。
 * 边界：不直接执行SQL；权限由Dispatcher按User路由完成。
 */
#pragma once

#include "network/messagedispatcher.h"

class StationService;

class StationHandler
{
public:
    explicit StationHandler(StationService *stationService);

    ResponseMessage listNearby(const RequestMessage &request,
                               const SessionContext &context);
    ResponseMessage detailGet(const RequestMessage &request,
                              const SessionContext &context);
    ResponseMessage recommendation(const RequestMessage &request,
                                   const SessionContext &context);
    void geocode(const RequestMessage &request, const SessionContext &context,
                 MessageDispatcher::ResponseCallback callback);
    void routePlan(const RequestMessage &request, const SessionContext &context,
                   MessageDispatcher::ResponseCallback callback);

private:
    StationService *m_stationService = nullptr;
};
