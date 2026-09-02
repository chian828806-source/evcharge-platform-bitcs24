#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>

class AdminManagementService
{
public:
    explicit AdminManagementService(QSqlDatabase database);
    ResponseMessage pileList(const RequestMessage &request) const;
    ResponseMessage restartPile(const RequestMessage &request, qint64 adminId) const;

private:
    QSqlDatabase m_database;
};
