#include "adminauthservice.h"

#include "common/passwordhasher.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlError>
#include <utility>

AdminAuthService::AdminAuthService(QSqlDatabase database, SessionManager *sessions)
    : m_database(database), m_sessions(sessions),
      m_adminRepository(database), m_logRepository(database)
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

    m_adminRepository.clearError();
    m_logRepository.clearError();
    const QJsonObject admin = m_adminRepository.findByUsername(username);
    if (admin.isEmpty() && !m_adminRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_adminRepository.lastError());
    }
    if (admin.isEmpty()
        || admin.value(QStringLiteral("status")).toString() != QStringLiteral("NORMAL")
        || !PasswordHasher::verifyPbkdf2Sha256(
            password, admin.value(QStringLiteral("passwordHash")).toString())) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidAdminCredentials,
                                      QStringLiteral("管理员账号或密码错误"));
    }

    const qint64 adminId = admin.value(QStringLiteral("adminId")).toInteger();
    const QString displayName = admin.value(QStringLiteral("displayName")).toString();
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    if (!m_adminRepository.updateLastLogin(adminId, now)
        || !m_logRepository.add(adminId, QStringLiteral("ADMIN_LOGIN"),
                                QStringLiteral("SYSTEM"), adminId, {}, {},
                                QStringLiteral("管理员登录成功"), now)
        || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_adminRepository.lastError()
                                          + m_logRepository.lastError());
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
