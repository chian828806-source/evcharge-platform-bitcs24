#pragma once

#include "repositorybase.h"

#include <QJsonArray>

class PileRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    QJsonArray statusSummary(int *total) const;
    QJsonArray list(qint64 stationId = 0) const;
    QString status(qint64 pileId, bool *found) const;
    bool compareAndSetStatus(qint64 pileId, const QString &before,
                             const QString &after, const QString &now) const;
    bool createForStation(qint64 stationId, int count, const QString &now) const;
};
