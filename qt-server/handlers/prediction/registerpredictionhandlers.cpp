#include "registerpredictionhandlers.h"
#include "predictionhandler.h"
#include "repositories/predictionrepository.h"
#include "services/prediction/predictionservice.h"
#include "shared/protocol/messagetypes.h"

PredictionHandlerRegistry::PredictionHandlerRegistry(DatabaseManager *databaseManager, MessageDispatcher *dispatcher)
    : m_repository(new PredictionRepository), m_service(new PredictionService(databaseManager, m_repository)), m_handler(new PredictionHandler(m_service))
{
    dispatcher->registerHandler(MessageTypes::PredictionList, MessageDispatcher::Access::AnyAuthenticated, [this](const RequestMessage &r, const SessionContext &c){ return m_handler->list(r,c); });
    dispatcher->registerHandler(MessageTypes::PredictionWarning, MessageDispatcher::Access::Admin, [this](const RequestMessage &r, const SessionContext &c){ return m_handler->warning(r,c); });
}
