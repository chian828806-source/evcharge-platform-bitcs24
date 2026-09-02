#include "adminauthservice.h"

#include "passwordhasher.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <utility>

AdminAuthService::AdminAuthService(QSqlDatabase database, SessionManager *sessions)
    : m_database(std::move(database)), m_sessions(sessions)
{
}

ResponseMessage AdminAuthService::login(const RequestMessage &request)
{
    const QString username = request.payload.value(QStringLiteral("username")).toString().trimmed();
    const QString password = request.payload.value(QStringLiteral("password")).toString();
    if (username.isEmpty() || password.isEmpty()) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidAdminCredentials,
                                      QStringLiteral("管理员账号或密码错误"));
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT id, password_hash, display_name, status FROM admin WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    if (!query.next()
        || query.value(3).toString() != QStringLiteral("NORMAL")
        || !PasswordHasher::verifyPbkdf2Sha256(password, query.value(1).toString())) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidAdminCredentials,
                                      QStringLiteral("管理员账号或密码错误"));
    }

    const qint64 adminId = query.value(0).toLongLong();
    const QString displayName = query.value(2).toString();
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral("UPDATE admin SET last_login_at = :now, updated_at = :now WHERE id = :id"));
    update.bindValue(QStringLiteral(":now"), now);
    update.bindValue(QStringLiteral(":id"), adminId);
    QSqlQuery log(m_database);
    log.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, result, message, created_at) VALUES(:id, 'ADMIN_LOGIN', 'SYSTEM', :id, 'SUCCESS', :message, :now)"));
    log.bindValue(QStringLiteral(":id"), adminId);
    log.bindValue(QStringLiteral(":message"), QStringLiteral("管理员登录成功"));
    log.bindValue(QStringLiteral(":now"), now);
    if (!update.exec() || !log.exec() || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    const QString sessionId = m_sessions->createSession(adminId, SessionRole::Admin);
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("admin"), QJsonObject{
             {QStringLiteral("adminId"), adminId},
             {QStringLiteral("username"), username},
             {QStringLiteral("displayName"), displayName}
         }}
    });
}
