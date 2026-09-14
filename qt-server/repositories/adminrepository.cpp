#include "adminrepository.h"

#include <QSqlError>
#include <QSqlQuery>

QJsonObject AdminRepository::findByUsername(const QString &username) const
{
    clearError();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT id, password_hash, display_name, status "
                                 "FROM admin WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return {};
    }
    if (!query.next()) return {};
    return {{QStringLiteral("adminId"), query.value(0).toLongLong()},
            {QStringLiteral("passwordHash"), query.value(1).toString()},
            {QStringLiteral("displayName"), query.value(2).toString()},
            {QStringLiteral("status"), query.value(3).toString()}};
}

bool AdminRepository::updateLastLogin(qint64 adminId, const QString &now) const
{
    clearError();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE admin SET last_login_at = :now, updated_at = :now "
                                 "WHERE id = :id"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":id"), adminId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}
