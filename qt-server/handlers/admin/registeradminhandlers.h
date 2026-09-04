#pragma once

#include "services/admin/adminanalyticsservice.h"
#include "services/admin/adminauthservice.h"
#include "services/admin/adminmanagementservice.h"

class MessageDispatcher;
class SessionManager;
class DatabaseManager;

class AdminHandlerRegistry
{
public:
    AdminHandlerRegistry(DatabaseManager *databaseManager, SessionManager *sessions,
                         MessageDispatcher *dispatcher);

private:
    AdminAuthService m_auth;
    AdminAnalyticsService m_analytics;
    AdminManagementService m_management;
};
