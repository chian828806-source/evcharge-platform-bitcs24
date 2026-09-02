#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QSqlDatabase>
#include <QObject>
#include "repositories/operationlogrepository.h"
#include "repositories/pilerepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"

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
    mutable QSqlDatabase m_database;
    PileRepository m_pileRepository;
    StationRepository m_stationRepository;
    UserRepository m_userRepository;
    OperationLogRepository m_logRepository;
};
