#include "adminanalyticsservice.h"
#include "database/databasemanager.h"
#include "repositories/pilerepository.h"
#include "shared/protocol/errorcodes.h"

namespace {
bool databaseFor(DatabaseManager *manager, QSqlDatabase *database, QString *error)
{ return manager && manager->database(database, error); }
}

AdminAnalyticsService::AdminAnalyticsService(DatabaseManager *databaseManager)
    : m_databaseManager(databaseManager)
{
}

ResponseMessage AdminAnalyticsService::revenueTrend(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error))
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError, error);
    const int days = request.payload.value(QStringLiteral("days")).toInt();
    const QJsonArray points = m_orderRepository.revenueTrend(
        database, QDate::currentDate().addDays(1 - days), days, &error);
    if (!error.isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      error);
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("days"), days},
                                     {QStringLiteral("points"), points}});
}

ResponseMessage AdminAnalyticsService::revenueSummary(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error))
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError, error);
    const QJsonObject summary = m_orderRepository.revenueSummary(database, QDate::currentDate(), &error);
    if (summary.isEmpty() && !error.isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      error);
    }
    return ResponseMessage::success(request.requestId, summary);
}

ResponseMessage AdminAnalyticsService::pileStatusSummary(const RequestMessage &request) const
{
    QSqlDatabase database; QString error;
    if (!databaseFor(m_databaseManager, &database, &error))
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError, error);
    PileRepository pileRepository(database);
    int total = 0;
    const QJsonArray statuses = pileRepository.statusSummary(&total);
    if (!pileRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      pileRepository.lastError());
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("total"), total},
                                     {QStringLiteral("statuses"), statuses}});
}
