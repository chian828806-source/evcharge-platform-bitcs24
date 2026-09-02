/*
 * 功能：实现Qt管理员端与独立业务服务端之间的异步TCP通信。
 * 协议：复用shared/protocol中的请求对象和JSON Lines编解码器。
 */
#include "adminsocketclient.h"

#include "shared/protocol/protocolmessage.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

AdminSocketClient::AdminSocketClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    // 将底层连接事件转换成管理员页面可直接订阅的信号。
    connect(m_socket, &QTcpSocket::connected,
            this, &AdminSocketClient::connected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &AdminSocketClient::disconnected);
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        failAllPending(QStringLiteral("socket disconnected"));
    });
    connect(m_socket, &QTcpSocket::readyRead,
            this, &AdminSocketClient::readAvailableData);
    // 页面只接收可展示的错误文本，不依赖底层Socket错误枚举。
    connect(m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit socketError(m_socket->errorString());
            });
}

void AdminSocketClient::connectToServer(const QString &host, quint16 port)
{
    // 每次切换服务端都清理旧连接和旧连接遗留的半帧。
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_codec.clear();
    m_socket->connectToHost(host, port);
}

void AdminSocketClient::disconnectFromServer()
{
    // Qt会先发送输出缓冲区中的数据，再发出disconnected信号。
    m_socket->disconnectFromHost();
}

bool AdminSocketClient::isConnected() const
{
    // 连接建立过程中不能写入业务请求。
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString AdminSocketClient::sendRequest(const QString &type,
                                       const QString &sessionId,
                                       const QJsonObject &payload,
                                       const QString &requestId, int timeoutMs)
{
    // 离线时立即通知页面，避免管理操作无期限等待。
    if (!isConnected()) {
        emit socketError(QStringLiteral("socket is not connected"));
        return {};
    }

    // 默认生成唯一ID；幂等重试时调用方可以显式复用原ID。
    const QString actualRequestId = requestId.isEmpty()
        ? QStringLiteral("REQ-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces)
        : requestId;
    const RequestMessage request{
        actualRequestId, type, sessionId, payload
    };
    if (m_socket->write(JsonLineCodec::encode(request.toJson())) < 0) {
        emit socketError(m_socket->errorString());
        return {};
    }

    clearPendingRequest(actualRequestId);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_pendingRequests.insert(actualRequestId, {
        type, QDateTime::currentDateTimeUtc(), timer
    });
    connect(timer, &QTimer::timeout, this, [this, actualRequestId]() {
        const auto iterator = m_pendingRequests.find(actualRequestId);
        if (iterator == m_pendingRequests.end()) {
            return;
        }
        const QString type = iterator->type;
        iterator->timer->deleteLater();
        m_pendingRequests.erase(iterator);
        emit requestTimedOut(actualRequestId, type);
    });
    timer->start(timeoutMs > 0 ? timeoutMs : 5000);
    return actualRequestId;
}

void AdminSocketClient::readAvailableData()
{
    // readyRead可能带来半条或多条响应，由独立Codec缓冲并分帧。
    bool overflow = false;
    const QList<QByteArray> frames = m_codec.append(m_socket->readAll(), &overflow);
    if (overflow) {
        emit protocolError(QStringLiteral("response buffer is too large"));
        m_socket->abort();
        return;
    }

    // 每个完整帧单独校验，单帧异常不会吞掉后续有效响应。
    for (const QByteArray &frame : frames) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(frame, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            emit protocolError(QStringLiteral("server returned invalid JSON"));
            continue;
        }

        // 管理页面只接收符合公共响应外壳的对象。
        const QJsonObject response = document.object();
        if (!response.value(QStringLiteral("requestId")).isString()
            || !response.value(QStringLiteral("code")).isDouble()
            || !response.value(QStringLiteral("message")).isString()
            || !response.value(QStringLiteral("data")).isObject()) {
            emit protocolError(QStringLiteral("server returned invalid response"));
            continue;
        }
        clearPendingRequest(response.value(QStringLiteral("requestId")).toString());
        emit responseReceived(response);
    }
}

void AdminSocketClient::clearPendingRequest(const QString &requestId)
{
    const auto iterator = m_pendingRequests.find(requestId);
    if (iterator == m_pendingRequests.end()) {
        return;
    }
    iterator->timer->stop();
    iterator->timer->deleteLater();
    m_pendingRequests.erase(iterator);
}

void AdminSocketClient::failAllPending(const QString &message)
{
    const auto pending = m_pendingRequests;
    m_pendingRequests.clear();
    for (auto iterator = pending.cbegin(); iterator != pending.cend(); ++iterator) {
        iterator->timer->stop();
        iterator->timer->deleteLater();
        emit requestFailed(iterator.key(), iterator->type, message);
    }
}
