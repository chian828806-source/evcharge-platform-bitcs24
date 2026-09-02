#include "adminanalyticsservice.h"
#include "shared/protocol/errorcodes.h"

AdminAnalyticsService::AdminAnalyticsService(QSqlDatabase database)
    : m_database(database), m_pileRepository(database)
{
}

ResponseMessage AdminAnalyticsService::revenueTrend(const RequestMessage &request) const
{
    const int days = request.payload.value(QStringLiteral("days")).toInt();
    QString error;
    const QJsonArray points = m_orderRepository.revenueTrend(
        m_database, QDate::currentDate().addDays(1 - days), days, &error);
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
    QString error;
    const QJsonObject summary = m_orderRepository.revenueSummary(m_database, QDate::currentDate(), &error);
    if (summary.isEmpty() && !error.isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      error);
    }
    return ResponseMessage::success(request.requestId, summary);
}

ResponseMessage AdminAnalyticsService::pileStatusSummary(const RequestMessage &request) const
{
    m_pileRepository.clearError();
    int total = 0;
    const QJsonArray statuses = m_pileRepository.statusSummary(&total);
    if (!m_pileRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_pileRepository.lastError());
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("total"), total},
                                     {QStringLiteral("statuses"), statuses}});
}
