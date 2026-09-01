/*
 * 功能：实现Qt用户端的异步TCP通信。
 * 协议：复用shared/protocol中的请求对象和JSON Lines编解码器。
 */
#include "socketclient.h"

#include "shared/protocol/protocolmessage.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QTcpSocket>
#include <QUuid>

SocketClient::SocketClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    // 将QTcpSocket原生事件转换为SocketClient对页面公开的信号。
    connect(m_socket, &QTcpSocket::connected,
            this, &SocketClient::connected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &SocketClient::disconnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &SocketClient::readAvailableData);
    // 页面不需要理解QAbstractSocket枚举，直接接收可显示的错误文本。
    connect(m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit socketError(m_socket->errorString());
            });
}

void SocketClient::connectToServer(const QString &host, quint16 port)
{
    // 切换服务端前先终止旧连接，避免两套字节流共用同一缓冲。
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    // 新连接不能继承旧连接遗留的半条消息。
    m_codec.clear();
    m_socket->connectToHost(host, port);
}

void SocketClient::disconnectFromServer()
{
    // disconnectFromHost会等待已排队数据写出，再发出disconnected信号。
    m_socket->disconnectFromHost();
}

bool SocketClient::isConnected() const
{
    // 只有ConnectedState才允许write，连接中状态不算可用。
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString SocketClient::sendRequest(const QString &type,
                                  const QString &sessionId,
                                  const QJsonObject &payload,
                                  const QString &requestId)
{
    // 提前拒绝离线发送，让页面立即获得错误而不是等待超时。
    if (!isConnected()) {
        emit socketError(QStringLiteral("socket is not connected"));
        return {};
    }

    // 默认生成唯一requestId；重试场景可以显式复用原ID。
    const QString actualRequestId = requestId.isEmpty()
        ? QStringLiteral("REQ-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces)
        : requestId;
    // 公共对象负责把空sessionId编码为JSON null。
    const RequestMessage request{
        actualRequestId, type, sessionId, payload
    };
    m_socket->write(JsonLineCodec::encode(request.toJson()));
    return actualRequestId;
}

void SocketClient::readAvailableData()
{
    // 一次readyRead可能得到半条或多条响应，统一交给Codec处理。
    bool overflow = false;
    const QList<QByteArray> frames = m_codec.append(m_socket->readAll(), &overflow);
    // 超大响应可能造成内存风险，因此报告协议错误并断开。
    if (overflow) {
        emit protocolError(QStringLiteral("response buffer is too large"));
        m_socket->abort();
        return;
    }

    // 每个完整帧独立解析，单条错误不会阻止后续帧处理。
    for (const QByteArray &frame : frames) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(frame, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            emit protocolError(QStringLiteral("server returned invalid JSON"));
            continue;
        }
        // 页面收到响应前先检查docs/03-API.md规定的四个公共字段。
        const QJsonObject response = document.object();
        if (!response.value(QStringLiteral("requestId")).isString()
            || !response.value(QStringLiteral("code")).isDouble()
            || !response.value(QStringLiteral("message")).isString()
            || !response.value(QStringLiteral("data")).isObject()) {
            emit protocolError(QStringLiteral("server returned invalid response"));
            continue;
        }
        // 业务页面再按requestId和code处理具体结果。
        emit responseReceived(response);
    }
}
