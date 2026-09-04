/*
 * 功能：将用户可见的站点查询消息注册为User权限路由。
 */
#include "registerstationhandlers.h"

#include "network/messagedispatcher.h"
#include "shared/protocol/messagetypes.h"
#include "stationhandler.h"

#include <utility>

void registerStationHandlers(MessageDispatcher *dispatcher, StationHandler *stationHandler)
{
    if (!dispatcher || !stationHandler) {
        return;
    }

    dispatcher->registerHandler(
        MessageTypes::StationListNearby, MessageDispatcher::Access::User,
        [stationHandler](const RequestMessage &request, const SessionContext &context) {
            return stationHandler->listNearby(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::StationDetailGet, MessageDispatcher::Access::User,
        [stationHandler](const RequestMessage &request, const SessionContext &context) {
            return stationHandler->detailGet(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::PredictionRecommendation, MessageDispatcher::Access::User,
        [stationHandler](const RequestMessage &request, const SessionContext &context) {
            return stationHandler->recommendation(request, context);
        });
    dispatcher->registerAsyncHandler(
        MessageTypes::MapGeocode, MessageDispatcher::Access::User,
        [stationHandler](const RequestMessage &request, const SessionContext &context,
                         MessageDispatcher::ResponseCallback callback) {
            stationHandler->geocode(request, context, std::move(callback));
        });
    dispatcher->registerAsyncHandler(
        MessageTypes::MapRoutePlan, MessageDispatcher::Access::User,
        [stationHandler](const RequestMessage &request, const SessionContext &context,
                         MessageDispatcher::ResponseCallback callback) {
            stationHandler->routePlan(request, context, std::move(callback));
        });
}
