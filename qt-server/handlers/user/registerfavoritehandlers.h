#pragma once

class FavoriteHandler;
class MessageDispatcher;

void registerFavoriteHandlers(MessageDispatcher *dispatcher, FavoriteHandler *handler);
