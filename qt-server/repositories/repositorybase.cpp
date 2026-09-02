#include "repositorybase.h"

#include <utility>

RepositoryBase::RepositoryBase(QSqlDatabase database)
    : m_database(std::move(database))
{
}

QString RepositoryBase::lastError() const
{
    return m_lastError;
}

void RepositoryBase::clearError() const
{
    m_lastError.clear();
}
