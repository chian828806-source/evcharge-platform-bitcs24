#pragma once

#include "network/sessionmanager.h"
#include "shared/protocol/protocolmessage.h"

class DatabaseManager;

class AdminAuthService
{
public:
    AdminAuthService(DatabaseManager *databaseManager, SessionManager *sessions);
    ResponseMessage login(const RequestMessage &request);

private:
    DatabaseManager *m_databaseManager = nullptr;
    SessionManager *m_sessions = nullptr;
};
