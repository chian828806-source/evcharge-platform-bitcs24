#pragma once

#include "repositorybase.h"

#include <QJsonObject>

class AdminRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    QJsonObject findByUsername(const QString &username) const;
    bool updateLastLogin(qint64 adminId, const QString &now) const;
};
