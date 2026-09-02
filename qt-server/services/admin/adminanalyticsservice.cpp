#include "adminanalyticsservice.h"
#include "shared/protocol/errorcodes.h"

AdminAnalyticsService::AdminAnalyticsService(QSqlDatabase database)
    : m_orderRepository(database), m_pileRepository(database)
{
}

ResponseMessage AdminAnalyticsService::revenueTrend(const RequestMessage &request) const
{
    const int days = request.payload.value(QStringLiteral("days")).toInt();
    m_orderRepository.clearError();
    const QJsonArray points = m_orderRepository.revenueTrend(
        QDate::currentDate().addDays(1 - days), days);
    if (!m_orderRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_orderRepository.lastError());
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("days"), days},
                                     {QStringLiteral("points"), points}});
}

ResponseMessage AdminAnalyticsService::revenueSummary(const RequestMessage &request) const
{
    m_orderRepository.clearError();
    const QJsonObject summary = m_orderRepository.revenueSummary(QDate::currentDate());
    if (summary.isEmpty() && !m_orderRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_orderRepository.lastError());
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
