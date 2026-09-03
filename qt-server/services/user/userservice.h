/*
 * 功能：实现普通用户登录、自动注册、资料读取和昵称修改规则。
 * 边界：不直接操作Socket；用户身份由Handler传入的SessionContext提供。
 */
#pragma once

#include "common/serviceresult.h"
#include "models/rechargeinfo.h"
#include "models/userprofile.h"

#include <QString>

class DatabaseManager;
class QSqlDatabase;
class UserRepository;

class UserService
{
public:
    UserService(DatabaseManager *databaseManager, UserRepository *userRepository,
                const QString &avatarDirectory = {});

    ServiceResult<UserProfile> login(const QString &phone);
    ServiceResult<UserProfile> profile(qint64 userId);
    ServiceResult<UserProfile> updateNickname(qint64 userId,
                                              const QString &nickname);
    ServiceResult<UserProfile> uploadAvatar(qint64 userId, const QString &fileName,
                                            const QString &mimeType,
                                            const QString &contentBase64);
    ServiceResult<RechargeInfo> recharge(qint64 userId, qint64 amountFen);

private:
    bool openDatabase(QSqlDatabase *database, QString *errorMessage) const;
    static bool isValidPhone(const QString &phone);

    DatabaseManager *m_databaseManager = nullptr;
    UserRepository *m_userRepository = nullptr;
    QString m_avatarDirectory;
};
