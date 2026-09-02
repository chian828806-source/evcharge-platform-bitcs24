#include "registeradminhandlers.h"

#include "network/messagedispatcher.h"
#include "shared/protocol/messagetypes.h"
#include "shared/protocol/errorcodes.h"

namespace {
ResponseMessage invalidPayload(const RequestMessage &request,
                               const QString &message)
{
    return ResponseMessage::error(request.requestId,
                                  ErrorCodes::InvalidSocketMessage, message);
}

bool isPositiveInteger(const QJsonObject &payload, const QString &name)
{
    const QJsonValue value = payload.value(name);
    return value.isDouble() && value.toInteger() > 0
        && double(value.toInteger()) == value.toDouble();
}
}

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
            if (!request.payload.value(QStringLiteral("username")).isString()
                || !request.payload.value(QStringLiteral("password")).isString()) {
                return invalidPayload(request,
                                      QStringLiteral("username and password must be strings"));
            }
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
            const QJsonValue days = request.payload.value(QStringLiteral("days"));
            if (!days.isDouble() || (days.toInt() != 7 && days.toInt() != 30)) {
                return invalidPayload(request, QStringLiteral("days must be 7 or 30"));
            }
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
            const QJsonValue stationId = request.payload.value(QStringLiteral("stationId"));
            if (!stationId.isUndefined() && !isPositiveInteger(request.payload,
                                                               QStringLiteral("stationId"))) {
                return invalidPayload(request, QStringLiteral("stationId must be a positive integer"));
            }
            return m_management.pileList(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminPileRestart, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &session) {
            if (!isPositiveInteger(request.payload, QStringLiteral("pileId"))) {
                return invalidPayload(request, QStringLiteral("pileId must be a positive integer"));
            }
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
            const QJsonObject &payload = request.payload;
            if (!payload.value(QStringLiteral("name")).isString()
                || !payload.value(QStringLiteral("address")).isString()
                || !payload.value(QStringLiteral("longitude")).isDouble()
                || !payload.value(QStringLiteral("latitude")).isDouble()
                || !isPositiveInteger(payload, QStringLiteral("pileCount"))) {
                return invalidPayload(request, QStringLiteral("invalid station payload"));
            }
            return m_management.createStation(request, session.principalId);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminUserList, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &) {
            const QJsonValue keyword = request.payload.value(QStringLiteral("phoneKeyword"));
            if (!keyword.isUndefined() && !keyword.isString()) {
                return invalidPayload(request, QStringLiteral("phoneKeyword must be a string"));
            }
            return m_management.userList(request);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminUserFreeze, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &session) {
            if (!isPositiveInteger(request.payload, QStringLiteral("userId"))) {
                return invalidPayload(request, QStringLiteral("userId must be a positive integer"));
            }
            return m_management.setUserFrozen(request, session.principalId, true);
        });
    dispatcher->registerHandler(
        MessageTypes::AdminUserUnfreeze, MessageDispatcher::Access::Admin,
        [this](const RequestMessage &request, const SessionContext &session) {
            if (!isPositiveInteger(request.payload, QStringLiteral("userId"))) {
                return invalidPayload(request, QStringLiteral("userId must be a positive integer"));
            }
            return m_management.setUserFrozen(request, session.principalId, false);
        });
}
