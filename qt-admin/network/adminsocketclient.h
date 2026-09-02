/*
 * 功能：为Qt管理员端页面提供统一TCP连接、请求发送和响应接收接口。
 * 边界：管理员页面只使用本类，不直接操作QTcpSocket或服务端数据库。
 */
#pragma once

#include "shared/protocol/jsonlinecodec.h"

#include <QJsonObject>
#include <QDateTime>
#include <QHash>
#include <QObject>

class QTcpSocket;
class QTimer;

// AdminSocketClient把底层异步Socket转换为管理员页面易用的Qt信号接口。
class AdminSocketClient : public QObject
{
    Q_OBJECT

public:
    // 创建内部QTcpSocket并连接底层事件信号。
    explicit AdminSocketClient(QObject *parent = nullptr);

    // 发起异步连接；连接结果通过信号返回，不阻塞界面线程。
    void connectToServer(const QString &host, quint16 port);
    // 请求Qt在已排队数据发送完成后断开连接。
    void disconnectFromServer();
    // 返回当前连接是否已经可以发送请求。
    bool isConnected() const;

    // 发送标准管理请求并返回requestId；未连接时返回空字符串。
    QString sendRequest(const QString &type, const QString &sessionId,
                        const QJsonObject &payload,
                        const QString &requestId = {}, int timeoutMs = 5000);

signals:
    // 管理页面订阅这些信号以更新连接状态和业务界面。
    void connected();
    void disconnected();
    void responseReceived(const QJsonObject &response);
    void protocolError(const QString &message);
    void socketError(const QString &message);
    void requestTimedOut(const QString &requestId, const QString &type);
    void requestFailed(const QString &requestId, const QString &type,
                       const QString &message);

private slots:
    // 读取服务端字节并拆成一条或多条完整响应。
    void readAvailableData();

private:
    // QTcpSocket和半包缓冲区都只属于本客户端实例及其线程。
    QTcpSocket *m_socket = nullptr;
    JsonLineCodec m_codec;

    struct PendingRequest {
        QString type;
        QDateTime sentAt;
        QTimer *timer = nullptr;
    };
    QHash<QString, PendingRequest> m_pendingRequests;

    void clearPendingRequest(const QString &requestId);
    void failAllPending(const QString &message);
};
