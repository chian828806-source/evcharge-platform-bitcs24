/*
 * 功能：实现RequestMessage和ResponseMessage的JSON映射。
 * 重点：统一null sessionId、空data对象和错误响应格式。
 */
#include "protocolmessage.h"

#include "errorcodes.h"

bool RequestMessage::fromJson(const QJsonObject &json, RequestMessage *message,
                              QString *errorMessage)
{
    // 输出指针无效时无法保存解析结果，直接失败。
    if (!message) {
        return false;
    }

    // 先取出必填字段，再逐项验证，避免隐式转换掩盖格式错误。
    const QJsonValue requestIdValue = json.value(QStringLiteral("requestId"));
    const QJsonValue typeValue = json.value(QStringLiteral("type"));
    const QJsonValue payloadValue = json.value(QStringLiteral("payload"));

    if (!requestIdValue.isString() || requestIdValue.toString().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("requestId must be a non-empty string");
        }
        return false;
    }
    if (!typeValue.isString() || typeValue.toString().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("type must be a non-empty string");
        }
        return false;
    }
    if (!payloadValue.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("payload must be an object");
        }
        return false;
    }

    // 登录请求允许sessionId为null，鉴权由Dispatcher负责。
    const QJsonValue sessionValue = json.value(QStringLiteral("sessionId"));
    if (sessionValue.isUndefined()
        || (!sessionValue.isNull() && !sessionValue.isString())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("sessionId is required and must be a string or null");
        }
        return false;
    }

    // 所有字段验证完成后才写入输出对象，避免半有效结果。
    message->requestId = requestIdValue.toString();
    message->type = typeValue.toString();
    message->sessionId = sessionValue.isString() ? sessionValue.toString()
                                                  : QString();
    message->payload = payloadValue.toObject();
    return true;
}

QJsonObject RequestMessage::toJson() const
{
    // 先写入请求都具有的结构字段。
    QJsonObject json{
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("type"), type},
        {QStringLiteral("payload"), payload}
    };
    // 空QString明确编码为JSON null，而不是空字符串。
    json.insert(QStringLiteral("sessionId"),
                sessionId.isEmpty() ? QJsonValue(QJsonValue::Null)
                                    : QJsonValue(sessionId));
    return json;
}

ResponseMessage ResponseMessage::success(const QString &requestId,
                                         const QJsonObject &data)
{
    // 成功码和默认消息集中生成，所有Handler保持一致。
    return {requestId, ErrorCodes::Success, QStringLiteral("success"), data};
}

ResponseMessage ResponseMessage::error(const QString &requestId, int code,
                                       const QString &message)
{
    // 当前契约要求data为对象，失败时使用空对象。
    return {requestId, code, message, {}};
}

QJsonObject ResponseMessage::toJson() const
{
    // 响应字段固定为requestId、code、message和data。
    return {
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), data}
    };
}
