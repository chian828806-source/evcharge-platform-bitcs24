/* 校验收藏消息payload并映射FavoriteService结果。 */
#pragma once

#include "network/messagedispatcher.h"

class FavoriteService;

class FavoriteHandler
{
public:
    explicit FavoriteHandler(FavoriteService *service);
    ResponseMessage toggle(const RequestMessage &request, const SessionContext &context);
    ResponseMessage list(const RequestMessage &request, const SessionContext &context);

private:
    FavoriteService *m_service = nullptr;
};
