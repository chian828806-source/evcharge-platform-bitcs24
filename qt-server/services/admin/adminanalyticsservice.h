#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>
#include "repositories/repositories.h"

class AdminAnalyticsService
{
public:
    explicit AdminAnalyticsService(QSqlDatabase database);
    ResponseMessage revenueSummary(const RequestMessage &request) const;
    ResponseMessage revenueTrend(const RequestMessage &request) const;
    ResponseMessage pileStatusSummary(const RequestMessage &request) const;

private:
    OrderRepository m_orderRepository;
    PileRepository m_pileRepository;
};
