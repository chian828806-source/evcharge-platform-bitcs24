#pragma once

#include "services/admin/adminanalyticsservice.h"
#include "services/admin/adminauthservice.h"
#include "services/admin/adminmanagementservice.h"

class MessageDispatcher;
class SessionManager;
class DatabaseManager;
class DeviceRegistry;
class DeviceControlService;

class AdminHandlerRegistry
{
public:
    AdminHandlerRegistry(DatabaseManager *databaseManager, SessionManager *sessions,
                         MessageDispatcher *dispatcher, DeviceRegistry *deviceRegistry = nullptr,
                         DeviceControlService *deviceControl = nullptr);

private:
    AdminAuthService m_auth;
    AdminAnalyticsService m_analytics;
    AdminManagementService m_management;
};
