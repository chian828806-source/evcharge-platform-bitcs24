#include "operationlogrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool OperationLogRepository::add(qint64 adminId, const QString &action,
                                 const QString &targetType, qint64 targetId,
                                 const QString &before, const QString &after,
                                 const QString &message, const QString &now) const
{
    clearError();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, "
                                 "before_status, after_status, result, message, created_at) "
                                 "VALUES(:adminId, :action, :targetType, :targetId, :before, :after, "
                                 "'SUCCESS', :message, :now)"));
    query.bindValue(QStringLiteral(":adminId"), adminId);
    query.bindValue(QStringLiteral(":action"), action);
    query.bindValue(QStringLiteral(":targetType"), targetType);
    query.bindValue(QStringLiteral(":targetId"), targetId);
    query.bindValue(QStringLiteral(":before"), before.isEmpty() ? QVariant() : QVariant(before));
    query.bindValue(QStringLiteral(":after"), after.isEmpty() ? QVariant() : QVariant(after));
    query.bindValue(QStringLiteral(":message"), message);
    query.bindValue(QStringLiteral(":now"), now);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}
