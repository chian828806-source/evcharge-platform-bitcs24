/*
 * 功能：定义Socket请求/响应的公共C++对象及JSON转换接口。
 * 边界：只校验消息外壳，payload中的业务字段由Handler或Service校验。
 */
#pragma once

#include <QJsonObject>
#include <QString>

// 客户端发送给服务端的标准请求。
struct RequestMessage {
    QString requestId;
    QString type;
    QString sessionId;
    QJsonObject payload;

    // 从JSON读取并校验四个公共字段。
    static bool fromJson(const QJsonObject &json, RequestMessage *message,
                         QString *errorMessage);
    // 将C++对象序列化为文档约定的请求对象。
    QJsonObject toJson() const;
};

// 服务端返回给客户端的标准响应。
struct ResponseMessage {
    QString requestId;
    int code = 0;
    QString message;
    QJsonObject data;

    // 构造code=200的成功响应。
    static ResponseMessage success(const QString &requestId,
                                   const QJsonObject &data = {});
    // 构造携带指定公共错误码的失败响应。
    static ResponseMessage error(const QString &requestId, int code,
                                 const QString &message);
    // 将响应转换为可编码的JSON对象。
    QJsonObject toJson() const;
};
