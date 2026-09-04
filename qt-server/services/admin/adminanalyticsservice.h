#pragma once

#include "shared/protocol/protocolmessage.h"

#include "repositories/orderrepository.h"
class DatabaseManager;

class AdminAnalyticsService
{
public:
    explicit AdminAnalyticsService(DatabaseManager *databaseManager);
    ResponseMessage revenueSummary(const RequestMessage &request) const;
    ResponseMessage revenueTrend(const RequestMessage &request) const;
    ResponseMessage pileStatusSummary(const RequestMessage &request) const;

private:
    DatabaseManager *m_databaseManager = nullptr;
    OrderRepository m_orderRepository;
};
