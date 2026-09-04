#pragma once

#include "shared/protocol/protocolmessage.h"

#include <QObject>
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
class DatabaseManager;

class AdminManagementService : public QObject
{
public:
    explicit AdminManagementService(DatabaseManager *databaseManager,
                                    QObject *parent = nullptr);
    ResponseMessage pileList(const RequestMessage &request) const;
    ResponseMessage restartPile(const RequestMessage &request, qint64 adminId) const;
    ResponseMessage stationList(const RequestMessage &request) const;
    ResponseMessage createStation(const RequestMessage &request, qint64 adminId) const;
    ResponseMessage userList(const RequestMessage &request) const;
    ResponseMessage setUserFrozen(const RequestMessage &request, qint64 adminId,
                                  bool frozen) const;

private:
    DatabaseManager *m_databaseManager = nullptr;
    StationRepository m_stationRepository;
    UserRepository m_userRepository;
};
