#include "adminauthservice.h"

#include "common/passwordhasher.h"
#include "database/databasemanager.h"
#include "repositories/adminrepository.h"
#include "repositories/operationlogrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlError>
#include <utility>

AdminAuthService::AdminAuthService(DatabaseManager *databaseManager, SessionManager *sessions)
    : m_databaseManager(databaseManager), m_sessions(sessions)
{
}

ResponseMessage AdminAuthService::login(const RequestMessage &request)
{
    QSqlDatabase database;
    QString databaseError;
    if (!m_databaseManager || !m_databaseManager->database(&database, &databaseError)) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      databaseError);
    }
    AdminRepository adminRepository(database);
    OperationLogRepository logRepository(database);
    const QString username = request.payload.value(QStringLiteral("username")).toString().trimmed();
    const QString password = request.payload.value(QStringLiteral("password")).toString();
    if (username.isEmpty() || password.isEmpty()) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidAdminCredentials,
                                      QStringLiteral("管理员账号或密码错误"));
    }

    const QJsonObject admin = adminRepository.findByUsername(username);
    if (admin.isEmpty() && !adminRepository.lastError().isEmpty()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      adminRepository.lastError());
    }
    if (admin.isEmpty()
        || admin.value(QStringLiteral("status")).toString() != QStringLiteral("NORMAL")
        || !PasswordHasher::verifyPbkdf2Sha256(
            password, admin.value(QStringLiteral("passwordHash")).toString())) {
        return ResponseMessage::error(request.requestId,
                                      ErrorCodes::InvalidAdminCredentials,
                                      QStringLiteral("管理员账号或密码错误"));
    }

    const qint64 adminId = static_cast<qint64>(
        admin.value(QStringLiteral("adminId")).toDouble());
    const QString displayName = admin.value(QStringLiteral("displayName")).toString();
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      database.lastError().text());
    }
    if (!adminRepository.updateLastLogin(adminId, now)
        || !logRepository.add(adminId, QStringLiteral("ADMIN_LOGIN"),
                                QStringLiteral("SYSTEM"), adminId, {}, {},
                                QStringLiteral("管理员登录成功"), now)
        || !database.commit()) {
        const QString error = adminRepository.lastError() + logRepository.lastError()
            + database.lastError().text();
        database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      error);
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
