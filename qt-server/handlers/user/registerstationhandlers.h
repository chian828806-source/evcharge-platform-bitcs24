/*
 * 功能：集中注册用户端站点查询路由。
 */
#pragma once

class MessageDispatcher;
class StationHandler;

void registerStationHandlers(MessageDispatcher *dispatcher, StationHandler *stationHandler);
