#pragma once
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>

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

class AdminRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    QJsonObject findByUsername(const QString &username) const;
    bool updateLastLogin(qint64 adminId, const QString &now) const;
};

class OrderRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    QJsonObject revenueSummary(const QDate &today) const;
    QJsonArray revenueTrend(const QDate &firstDate, int days) const;
};

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

class StationRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    QJsonArray list() const;
    qint64 create(const QJsonObject &values, const QString &stationNo,
                  const QString &now) const;
};

class UserRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    QJsonArray list(const QString &phoneKeyword) const;
    QJsonObject findById(qint64 userId) const;
    bool compareAndSetStatus(qint64 userId, const QString &before,
                             const QString &after, const QString &now) const;
};

class OperationLogRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    bool add(qint64 adminId, const QString &action, const QString &targetType,
             qint64 targetId, const QString &before, const QString &after,
             const QString &message, const QString &now) const;
};
