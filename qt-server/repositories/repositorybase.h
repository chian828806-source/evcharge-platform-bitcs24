#pragma once

#include <QSqlDatabase>
#include <QString>

class RepositoryBase
{
public:
    explicit RepositoryBase(QSqlDatabase database);

    QString lastError() const;
    void clearError() const;

protected:
    QSqlDatabase m_database;
    mutable QString m_lastError;
};
