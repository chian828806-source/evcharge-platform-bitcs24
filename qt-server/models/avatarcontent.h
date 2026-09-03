/*
 * 功能：定义头像读取接口的安全传输对象。
 * 边界：只携带已校验的相对路径、MIME 与 Base64 内容，不泄露服务端绝对路径。
 */
#pragma once

#include <QJsonObject>
#include <QString>

struct AvatarContent
{
    QString avatarPath;
    QString mimeType;
    QString contentBase64;

    QJsonObject toJson() const
    {
        QJsonObject json;
        json.insert(QStringLiteral("avatarPath"), avatarPath.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(avatarPath));
        json.insert(QStringLiteral("mimeType"), mimeType.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(mimeType));
        json.insert(QStringLiteral("contentBase64"), contentBase64.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(contentBase64));
        return json;
    }
};
