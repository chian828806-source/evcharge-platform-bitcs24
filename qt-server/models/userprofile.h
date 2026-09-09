/*
 * 功能：表示服务端与客户端之间传递的普通用户资料。
 * 边界：不保存数据库连接；字段名通过toJson统一转换为Socket契约的camelCase。
 */
#pragma once

#include <QJsonObject>
#include <QString>

struct UserProfile
{
    qint64 userId = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    qint64 balanceFen = 0;
    bool isMember = false;
    int membershipRemainingDays = 0;
    QString membershipExpiresAt;
    int membershipDiscountBps = 10000;
    QString status;
    QString createdAt;

    QJsonObject toJson() const
    {
        QJsonObject json{
            {QStringLiteral("userId"), userId},
            {QStringLiteral("phone"), phone},
            {QStringLiteral("nickname"), nickname},
            {QStringLiteral("balanceFen"), balanceFen},
            {QStringLiteral("isMember"), isMember},
            {QStringLiteral("membershipRemainingDays"), membershipRemainingDays},
            {QStringLiteral("membershipExpiresAt"), membershipExpiresAt},
            {QStringLiteral("membershipDiscountBps"), membershipDiscountBps},
            {QStringLiteral("status"), status},
            {QStringLiteral("createdAt"), createdAt}
        };
        json.insert(QStringLiteral("avatarPath"),
                    avatarPath.isEmpty()
                        ? QJsonValue(QJsonValue::Null)
                        : QJsonValue(avatarPath));
        return json;
    }
};
