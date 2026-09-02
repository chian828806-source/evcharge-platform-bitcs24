#pragma once

#include "repositorybase.h"

class OperationLogRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool add(qint64 adminId, const QString &action, const QString &targetType,
             qint64 targetId, const QString &before, const QString &after,
             const QString &message, const QString &now) const;
};
