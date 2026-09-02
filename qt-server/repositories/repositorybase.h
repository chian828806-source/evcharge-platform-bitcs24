#pragma once

#include <QSqlDatabase>
#include <QString>

class RepositoryBase
{
public:
    QString lastError() const;
    void clearError() const;

protected:
    explicit RepositoryBase(QSqlDatabase database);

    QSqlDatabase m_database;
    mutable QString m_lastError;
};
