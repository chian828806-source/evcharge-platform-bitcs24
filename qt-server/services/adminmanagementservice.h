#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>

class AdminManagementService
{
public:
    explicit AdminManagementService(QSqlDatabase database);
    ResponseMessage pileList(const RequestMessage &request) const;
    ResponseMessage restartPile(const RequestMessage &request, qint64 adminId) const;
    ResponseMessage stationList(const RequestMessage &request) const;

private:
    QSqlDatabase m_database;
};
