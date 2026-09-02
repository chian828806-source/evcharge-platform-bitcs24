/*
 * 功能：实现用户端首个业务闭环。
 */
#include "userservice.h"

#include "database/databasemanager.h"
#include "repositories/userrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlDatabase>

UserService::UserService(DatabaseManager *databaseManager,
                         UserRepository *userRepository)
    : m_databaseManager(databaseManager), m_userRepository(userRepository)
{
}

ServiceResult<UserProfile> UserService::login(const QString &phone)
{
    const QString normalizedPhone = phone.trimmed();
    if (!isValidPhone(normalizedPhone)) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InvalidPhone, QStringLiteral("invalid phone"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    if (!database.transaction()) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("cannot start transaction"));
    }

    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    auto user = m_userRepository->findByPhone(database, normalizedPhone, &databaseError);
    if (!user.has_value() && !databaseError.isEmpty()) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("query user failed"));
    }

    if (!user.has_value()) {
        if (!m_userRepository->createAutoRegistered(database, normalizedPhone, now,
                                                    &databaseError)) {
            database.rollback();
            return ServiceResult<UserProfile>::failure(
                ErrorCodes::DatabaseError, QStringLiteral("automatic registration failed"));
        }
        user = m_userRepository->findByPhone(database, normalizedPhone, &databaseError);
        if (!user.has_value()) {
            database.rollback();
            return ServiceResult<UserProfile>::failure(
                ErrorCodes::DatabaseError, QStringLiteral("read new user failed"));
        }
    }

    if (user->status == QStringLiteral("FROZEN")) {
        bool hasActiveOrder = false;
        if (!m_userRepository->hasActiveOrder(database, user->userId,
                                              &hasActiveOrder, &databaseError)) {
            database.rollback();
            return ServiceResult<UserProfile>::failure(
                ErrorCodes::DatabaseError, QStringLiteral("check active order failed"));
        }
        if (!hasActiveOrder) {
            database.rollback();
            return ServiceResult<UserProfile>::failure(
                ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
        }
    }

    if (!m_userRepository->updateLastLogin(database, user->userId, now,
                                           &databaseError)
        || !database.commit()) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("update login time failed"));
    }

    return ServiceResult<UserProfile>::success(*user);
}

ServiceResult<UserProfile> UserService::profile(qint64 userId)
{
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }

    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value()) {
        return ServiceResult<UserProfile>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession
                                    : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("user no longer exists")
                                    : QStringLiteral("query user failed"));
    }
    return ServiceResult<UserProfile>::success(*user);
}

ServiceResult<UserProfile> UserService::updateNickname(qint64 userId,
                                                        const QString &nickname)
{
    const QString normalizedNickname = nickname.trimmed();
    if (normalizedNickname.size() < 2 || normalizedNickname.size() > 20) {
        // 当前公共契约尚未定义独立昵称错误码，暂按业务payload不合法返回4401。
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InvalidSocketMessage,
            QStringLiteral("nickname length must be between 2 and 20"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    if (!database.transaction()) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("cannot start transaction"));
    }

    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_userRepository->updateNickname(database, userId, normalizedNickname,
                                          now, &databaseError)) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("update nickname failed"));
    }

    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value() || !database.commit()) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read updated user failed"));
    }
    return ServiceResult<UserProfile>::success(*user);
}

bool UserService::openDatabase(QSqlDatabase *database,
                               QString *errorMessage) const
{
    return m_databaseManager && m_userRepository
        && m_databaseManager->database(database, errorMessage);
}

bool UserService::isValidPhone(const QString &phone)
{
    static const QRegularExpression phonePattern(
        QStringLiteral("^1[0-9]{10}$"));
    return phonePattern.match(phone).hasMatch();
}
