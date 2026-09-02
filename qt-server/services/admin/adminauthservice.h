#pragma once

#include "network/sessionmanager.h"
#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>
#include "repositories/repositories.h"

class AdminAuthService
{
public:
    AdminAuthService(QSqlDatabase database, SessionManager *sessions);
    ResponseMessage login(const RequestMessage &request);

private:
    QSqlDatabase m_database;
    SessionManager *m_sessions = nullptr;
    AdminRepository m_adminRepository;
    OperationLogRepository m_logRepository;
};
