#pragma once

#include "repositorybase.h"

#include <optional>

class OperationLogRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool add(std::optional<qint64> adminId, const QString &action, const QString &targetType,
             qint64 targetId, const QString &before, const QString &after,
             const QString &message, const QString &now) const;
};
