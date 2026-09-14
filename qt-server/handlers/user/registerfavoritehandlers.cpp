#include "registerfavoritehandlers.h"

#include "favoritehandler.h"
#include "network/messagedispatcher.h"
#include "shared/protocol/messagetypes.h"

void registerFavoriteHandlers(MessageDispatcher *dispatcher, FavoriteHandler *handler)
{
    if (!dispatcher || !handler) return;
    dispatcher->registerHandler(
        MessageTypes::UserStationFavoriteToggle, MessageDispatcher::Access::User,
        [handler](const RequestMessage &request, const SessionContext &context) {
            return handler->toggle(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::UserStationFavoriteList, MessageDispatcher::Access::User,
        [handler](const RequestMessage &request, const SessionContext &context) {
            return handler->list(request, context);
        });
}
