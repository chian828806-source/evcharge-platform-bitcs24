#pragma once

#include "repositorybase.h"

#include <QJsonArray>
#include <optional>

class PileRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    struct DeviceState { QString status; qint64 currentOrderId = 0; };

    QJsonArray statusSummary(int *total) const;
    QJsonArray list(qint64 stationId = 0) const;
    QString status(qint64 pileId, bool *found) const;
    bool compareAndSetStatus(qint64 pileId, const QString &before,
                             const QString &after, const QString &now) const;
    bool createForStation(qint64 stationId, int count, const QString &now) const;
    bool deviceHello(qint64 pileId, const QString &pileNo, double *ratedPower,
                     QString *status) const;
    bool updateHeartbeat(qint64 pileId, const QString &now) const;
    std::optional<DeviceState> deviceState(qint64 pileId) const;
    bool transitionDeviceState(qint64 pileId, const QString &before, const QString &after,
                               const QString &now, qint64 expectedOrderId = 0) const;
};
