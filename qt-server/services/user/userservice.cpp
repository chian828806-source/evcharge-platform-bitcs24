/*
 * 功能：实现用户端首个业务闭环。
 */
#include "userservice.h"

#include "database/databasemanager.h"
#include "repositories/userrepository.h"
#include "shared/protocol/errorcodes.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QUuid>

UserService::UserService(DatabaseManager *databaseManager,
                         UserRepository *userRepository,
                         const QString &avatarDirectory)
    : m_databaseManager(databaseManager), m_userRepository(userRepository),
      m_avatarDirectory(avatarDirectory.isEmpty()
          ? QDir::current().filePath(QStringLiteral("data/avatars"))
          : avatarDirectory)
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

    const auto existingUser = m_userRepository->findById(database, userId, &databaseError);
    if (!existingUser.has_value()) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("user no longer exists")
                                    : QStringLiteral("query user failed"));
    }
    if (existingUser->status != QStringLiteral("NORMAL")) {
        database.rollback();
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::UserFrozen, QStringLiteral("user is frozen"));
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

ServiceResult<UserProfile> UserService::uploadAvatar(
    qint64 userId, const QString &fileName, const QString &mimeType,
    const QString &contentBase64)
{
    const QString suffix = QFileInfo(fileName.trimmed()).suffix().toLower();
    const QString mime = mimeType.trimmed().toLower();
    const bool png = suffix == QStringLiteral("png") && mime == QStringLiteral("image/png");
    const bool jpeg = (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))
        && mime == QStringLiteral("image/jpeg");
    if (!png && !jpeg) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InvalidSocketMessage,
            QStringLiteral("only png or jpeg avatars are supported"));
    }

    const QByteArray content = QByteArray::fromBase64(
        contentBase64.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    constexpr qsizetype maxAvatarBytes = 512 * 1024;
    const bool validPng = content.startsWith("\x89PNG\r\n\x1a\n");
    const bool validJpeg = content.size() >= 3
        && static_cast<unsigned char>(content.at(0)) == 0xff
        && static_cast<unsigned char>(content.at(1)) == 0xd8
        && static_cast<unsigned char>(content.at(2)) == 0xff;
    if (content.isEmpty() || content.size() > maxAvatarBytes
        || (png && !validPng) || (jpeg && !validJpeg)) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InvalidSocketMessage,
            QStringLiteral("invalid avatar content"));
    }

    QDir avatarDirectory(m_avatarDirectory);
    if (!avatarDirectory.mkpath(QStringLiteral("."))) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InternalError, QStringLiteral("cannot create avatar directory"));
    }
    const QString storedSuffix = png ? QStringLiteral("png") : QStringLiteral("jpg");
    const QString generatedName = QStringLiteral("user-%1-%2.%3")
        .arg(userId)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        .arg(storedSuffix);
    const QString absolutePath = avatarDirectory.filePath(generatedName);
    QSaveFile avatarFile(absolutePath);
    if (!avatarFile.open(QIODevice::WriteOnly)
        || avatarFile.write(content) != content.size()
        || !avatarFile.commit()) {
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::InternalError, QStringLiteral("cannot save avatar file"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        QFile::remove(absolutePath);
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto previousUser = m_userRepository->findById(database, userId, &databaseError);
    if (!previousUser.has_value() || previousUser->status != QStringLiteral("NORMAL")) {
        database.rollback();
        QFile::remove(absolutePath);
        return ServiceResult<UserProfile>::failure(
            previousUser.has_value() ? ErrorCodes::UserFrozen
                                     : (databaseError.isEmpty() ? ErrorCodes::InvalidSession
                                                                : ErrorCodes::DatabaseError),
            previousUser.has_value() ? QStringLiteral("user is frozen")
                                     : QStringLiteral("user is unavailable"));
    }

    const QString now = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString relativePath = QStringLiteral("avatars/") + generatedName;
    if (!m_userRepository->updateAvatarPath(database, userId, relativePath, now,
                                            &databaseError)) {
        database.rollback();
        QFile::remove(absolutePath);
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("update avatar failed"));
    }
    const auto updatedUser = m_userRepository->findById(database, userId, &databaseError);
    if (!updatedUser.has_value() || !database.commit()) {
        database.rollback();
        QFile::remove(absolutePath);
        return ServiceResult<UserProfile>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("read updated avatar failed"));
    }

    const QString oldPath = previousUser->avatarPath;
    if (oldPath.startsWith(QStringLiteral("avatars/"))) {
        const QString oldName = QFileInfo(oldPath).fileName();
        if (!oldName.isEmpty()) {
            QFile::remove(avatarDirectory.filePath(oldName));
        }
    }
    return ServiceResult<UserProfile>::success(*updatedUser);
}

ServiceResult<AvatarContent> UserService::avatarContent(qint64 userId)
{
    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError)) {
        return ServiceResult<AvatarContent>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value()) {
        return ServiceResult<AvatarContent>::failure(
            databaseError.isEmpty() ? ErrorCodes::InvalidSession : ErrorCodes::DatabaseError,
            databaseError.isEmpty() ? QStringLiteral("user no longer exists")
                                    : QStringLiteral("query user failed"));
    }

    AvatarContent avatar;
    avatar.avatarPath = user->avatarPath;
    if (avatar.avatarPath.isEmpty()) {
        return ServiceResult<AvatarContent>::success(avatar);
    }
    if (!avatar.avatarPath.startsWith(QStringLiteral("avatars/"))) {
        return ServiceResult<AvatarContent>::failure(
            ErrorCodes::InternalError, QStringLiteral("invalid stored avatar path"));
    }

    const QString fileName = QFileInfo(avatar.avatarPath).fileName();
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (fileName.isEmpty() || (suffix != QStringLiteral("png")
        && suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg"))) {
        return ServiceResult<AvatarContent>::failure(
            ErrorCodes::InternalError, QStringLiteral("invalid stored avatar file"));
    }
    QFile avatarFile(QDir(m_avatarDirectory).filePath(fileName));
    if (!avatarFile.open(QIODevice::ReadOnly)) {
        return ServiceResult<AvatarContent>::failure(
            ErrorCodes::InternalError, QStringLiteral("avatar file is unavailable"));
    }
    const QByteArray content = avatarFile.readAll();
    constexpr qsizetype maxAvatarBytes = 512 * 1024;
    const bool validPng = content.startsWith("\x89PNG\r\n\x1a\n");
    const bool validJpeg = content.size() >= 3
        && static_cast<unsigned char>(content.at(0)) == 0xff
        && static_cast<unsigned char>(content.at(1)) == 0xd8
        && static_cast<unsigned char>(content.at(2)) == 0xff;
    if (content.isEmpty() || content.size() > maxAvatarBytes
        || (suffix == QStringLiteral("png") && !validPng)
        || (suffix != QStringLiteral("png") && !validJpeg)) {
        return ServiceResult<AvatarContent>::failure(
            ErrorCodes::InternalError, QStringLiteral("stored avatar content is invalid"));
    }
    avatar.mimeType = suffix == QStringLiteral("png")
        ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    avatar.contentBase64 = QString::fromLatin1(content.toBase64());
    return ServiceResult<AvatarContent>::success(avatar);
}

ServiceResult<RechargeInfo> UserService::recharge(qint64 userId, qint64 amountFen)
{
    constexpr qint64 maxRechargeFen = 100000000;
    if (amountFen <= 0 || amountFen > maxRechargeFen) {
        return ServiceResult<RechargeInfo>::failure(
            ErrorCodes::InvalidSocketMessage, QStringLiteral("invalid recharge amount"));
    }

    QSqlDatabase database;
    QString databaseError;
    if (!openDatabase(&database, &databaseError) || !database.transaction()) {
        return ServiceResult<RechargeInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("database unavailable"));
    }
    const auto user = m_userRepository->findById(database, userId, &databaseError);
    if (!user.has_value() || user->status != QStringLiteral("NORMAL")) {
        database.rollback();
        return ServiceResult<RechargeInfo>::failure(
            user.has_value() ? ErrorCodes::UserFrozen
                             : (databaseError.isEmpty() ? ErrorCodes::InvalidSession
                                                        : ErrorCodes::DatabaseError),
            user.has_value() ? QStringLiteral("user is frozen")
                             : QStringLiteral("user is unavailable"));
    }

    RechargeInfo recharge;
    recharge.amountFen = amountFen;
    recharge.createdAt = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    recharge.recordNo = QStringLiteral("R-%1-%2")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")))
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (!m_userRepository->increaseBalance(database, userId, amountFen,
                                           recharge.createdAt, &recharge.balanceFen,
                                           &databaseError)
        || !m_userRepository->insertRechargeRecord(database, userId, recharge,
                                                    &recharge.rechargeId, &databaseError)
        || !database.commit()) {
        database.rollback();
        return ServiceResult<RechargeInfo>::failure(
            ErrorCodes::DatabaseError, QStringLiteral("recharge transaction failed"));
    }
    return ServiceResult<RechargeInfo>::success(recharge);
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
