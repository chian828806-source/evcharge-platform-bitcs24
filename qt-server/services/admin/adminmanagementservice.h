#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>
#include <QObject>

class AdminManagementService : public QObject
{
public:
    explicit AdminManagementService(QSqlDatabase database,
                                    QObject *parent = nullptr);
    ResponseMessage pileList(const RequestMessage &request) const;
    ResponseMessage restartPile(const RequestMessage &request, qint64 adminId) const;
    ResponseMessage stationList(const RequestMessage &request) const;
    ResponseMessage createStation(const RequestMessage &request, qint64 adminId) const;
    ResponseMessage userList(const RequestMessage &request) const;
    ResponseMessage setUserFrozen(const RequestMessage &request, qint64 adminId,
                                  bool frozen) const;

private:
    QSqlDatabase m_database;
};
