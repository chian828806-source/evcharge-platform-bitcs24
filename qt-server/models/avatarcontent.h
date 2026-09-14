/*
 * 功能：承载已鉴权头像读取结果。
 * 边界：只保存内存中的图片内容；不向客户端暴露服务端绝对路径。
 */
#pragma once

#include <QByteArray>
#include <QString>

struct AvatarContent
{
    QString avatarPath;
    QString mimeType;
    QByteArray content;
};
