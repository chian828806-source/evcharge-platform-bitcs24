/*
 * 功能：实现单个TCP连接的JSON Lines接收循环。
 * 边界：只处理协议与分发，不包含登录、订单或数据库业务。
 */
#include "clientsession.h"

#include "messagedispatcher.h"
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/protocolmessage.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QTcpSocket>

ClientSession::ClientSession(QTcpSocket *socket,
                             MessageDispatcher *dispatcher, QObject *parent)
    : QObject(parent), m_socket(socket), m_dispatcher(dispatcher)
{
    // Socket转交给ClientSession管理，确保两个对象一起释放。
    Q_ASSERT(m_socket);
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientSession::readAvailableData);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientSession::closeSession);
}

void ClientSession::readAvailableData()
{
    // readAll可能得到半包或粘包，codec负责保留尾部并输出完整帧。
    bool overflow = false;
    const QList<QByteArray> frames = m_codec.append(m_socket->readAll(), &overflow);
    // 超限连接可能持续占用内存，返回错误后主动断开。
    if (overflow) {
        sendProtocolError({}, QStringLiteral("message buffer is too large"));
        m_socket->disconnectFromHost();
        return;
    }
    // 一次readyRead可能产生多条请求，逐条独立处理和响应。
    for (const QByteArray &frame : frames) {
        processFrame(frame);
    }
}

void ClientSession::closeSession()
{
    // 使用deleteLater避免在Qt信号回调栈中立即销毁sender。
    deleteLater();
}

void ClientSession::processFrame(const QByteArray &frame)
{
    // 第一阶段检查字节是否为完整JSON对象。
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(frame, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        sendProtocolError({}, QStringLiteral("invalid JSON message"));
        return;
    }

    // 第二阶段检查requestId、type、sessionId和payload公共外壳。
    RequestMessage request;
    QString validationError;
    if (!RequestMessage::fromJson(document.object(), &request, &validationError)) {
        const QString requestId =
            document.object().value(QStringLiteral("requestId")).toString();
        sendProtocolError(requestId, validationError);
        return;
    }

    // 只有公共解析成功的请求才进入权限与业务分发。
    const ResponseMessage response = m_dispatcher
        ? m_dispatcher->dispatch(request)
        : ResponseMessage::error(request.requestId, ErrorCodes::InternalError,
                                 QStringLiteral("dispatcher is unavailable"));
    // 响应重新编码为一行JSON，客户端可用同一Codec处理。
    m_socket->write(JsonLineCodec::encode(response.toJson()));
}

void ClientSession::sendProtocolError(const QString &requestId,
                                      const QString &message)
{
    // 无法解析requestId时允许传空字符串，但响应结构仍保持完整。
    const ResponseMessage response = ResponseMessage::error(
        requestId, ErrorCodes::InvalidSocketMessage, message);
    m_socket->write(JsonLineCodec::encode(response.toJson()));
}
