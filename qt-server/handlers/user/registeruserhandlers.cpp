/*
 * 功能：把首批用户消息绑定到UserHandler，并声明正确的公共访问级别。
 */
#include "registeruserhandlers.h"

#include "network/messagedispatcher.h"
#include "shared/protocol/messagetypes.h"
#include "userhandler.h"

void registerUserHandlers(MessageDispatcher *dispatcher, UserHandler *userHandler)
{
    if (!dispatcher || !userHandler) {
        return;
    }

    dispatcher->registerHandler(
        MessageTypes::UserLogin, MessageDispatcher::Access::Public,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->login(request, context);
        });

    dispatcher->registerHandler(
        MessageTypes::UserProfileGet, MessageDispatcher::Access::User,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->profileGet(request, context);
        });

    dispatcher->registerHandler(
        MessageTypes::UserProfileUpdate, MessageDispatcher::Access::User,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->profileUpdate(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::UserAvatarUpload, MessageDispatcher::Access::User,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->avatarUpload(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::UserAvatarGet, MessageDispatcher::Access::User,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->avatarGet(request, context);
        });
    dispatcher->registerHandler(
        MessageTypes::UserRecharge, MessageDispatcher::Access::User,
        [userHandler](const RequestMessage &request, const SessionContext &context) {
            return userHandler->recharge(request, context);
        });
}
