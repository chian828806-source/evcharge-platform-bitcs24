#include "adminanalyticsservice.h"

#include "shared/protocol/errorcodes.h"

#include <QDate>
#include <QJsonArray>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <utility>

AdminAnalyticsService::AdminAnalyticsService(QSqlDatabase database)
    : m_database(std::move(database))
{
}

ResponseMessage AdminAnalyticsService::revenueTrend(const RequestMessage &request) const
{
    const int days = request.payload.value(QStringLiteral("days")).toInt(7);
    if (days != 7 && days != 30) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("days must be 7 or 30"));
    }
    const QDate firstDate = QDate::currentDate().addDays(1 - days);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT substr(paid_at, 1, 10), COALESCE(SUM(amount_fen), 0) "
        "FROM charging_order WHERE status = 'COMPLETED' AND paid_at >= :start "
        "GROUP BY substr(paid_at, 1, 10)"));
    query.bindValue(QStringLiteral(":start"),
                    firstDate.toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    if (!query.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QHash<QString, qint64> revenueByDate;
    while (query.next()) {
        revenueByDate.insert(query.value(0).toString(), query.value(1).toLongLong());
    }
    QJsonArray points;
    for (int offset = 0; offset < days; ++offset) {
        const QString date = firstDate.addDays(offset).toString(Qt::ISODate);
        points.append(QJsonObject{
            {QStringLiteral("date"), date},
            {QStringLiteral("revenueFen"), revenueByDate.value(date, 0)}
        });
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("days"), days},
        {QStringLiteral("points"), points}
    });
}

ResponseMessage AdminAnalyticsService::revenueSummary(const RequestMessage &request) const
{
    const QDate today = QDate::currentDate();
    const QString dayStart = today.toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString dayEnd = today.addDays(1).toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString monthStart = QDate(today.year(), today.month(), 1)
                                   .toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT "
        "COALESCE(SUM(CASE WHEN paid_at >= :dayStart AND paid_at < :dayEnd THEN amount_fen ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN paid_at >= :monthStart AND paid_at < :dayEnd THEN amount_fen ELSE 0 END), 0), "
        "COALESCE(SUM(amount_fen), 0) "
        "FROM charging_order WHERE status = 'COMPLETED'"));
    query.bindValue(QStringLiteral(":dayStart"), dayStart);
    query.bindValue(QStringLiteral(":dayEnd"), dayEnd);
    query.bindValue(QStringLiteral(":monthStart"), monthStart);
    if (!query.exec() || !query.next()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("todayRevenueFen"), query.value(0).toLongLong()},
        {QStringLiteral("monthRevenueFen"), query.value(1).toLongLong()},
        {QStringLiteral("totalRevenueFen"), query.value(2).toLongLong()}
    });
}
