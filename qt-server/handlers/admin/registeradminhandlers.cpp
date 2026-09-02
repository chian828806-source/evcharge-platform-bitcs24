#include "registeradminhandlers.h"

#include "network/messagedispatcher.h"
#include "shared/protocol/messagetypes.h"

AdminHandlerRegistry::AdminHandlerRegistry(QSqlDatabase database,
                                           SessionManager *sessions,
                                           MessageDispatcher *dispatcher)
    : m_auth(database, sessions),
      m_analytics(database),
      m_management(database)
{
    dispatcher->registerHandler(
        MessageTypes::AdminLogin, MessageDispatcher::Access::Public,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_auth.login(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminRevenueSummary, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_analytics.revenueSummary(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminRevenueTrend, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_analytics.revenueTrend(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminPileStatusSummary, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_analytics.pileStatusSummary(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminPileList, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_management.pileList(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminPileRestart, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &session) {
            return m_management.restartPile(request, session.principalId);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminStationList, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_management.stationList(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminStationCreate, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &session) {
            return m_management.createStation(request, session.principalId);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminUserList, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            return m_management.userList(request);
        });
}
