#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>

class AdminAnalyticsService
{
public:
    explicit AdminAnalyticsService(QSqlDatabase database);
    ResponseMessage revenueSummary(const RequestMessage &request) const;
    ResponseMessage revenueTrend(const RequestMessage &request) const;
    ResponseMessage pileStatusSummary(const RequestMessage &request) const;

private:
    QSqlDatabase m_database;
};
