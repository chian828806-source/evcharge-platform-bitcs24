#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>

class AdminManagementService
{
public:
    explicit AdminManagementService(QSqlDatabase database);
    ResponseMessage pileList(const RequestMessage &request) const;

private:
    QSqlDatabase m_database;
};
