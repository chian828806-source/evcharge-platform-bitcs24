#pragma once
#include "network/messagedispatcher.h"
class DatabaseManager; class PredictionHandler; class PredictionRepository; class PredictionService;
class PredictionHandlerRegistry
{
public:
    PredictionHandlerRegistry(DatabaseManager *databaseManager, MessageDispatcher *dispatcher);
private:
    PredictionRepository *m_repository; PredictionService *m_service; PredictionHandler *m_handler;
};
