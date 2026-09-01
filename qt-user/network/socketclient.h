/*
 * 功能：为Qt用户端页面提供统一TCP连接、请求发送和响应接收接口。
 * 边界：页面不直接操作QTcpSocket，业务含义由页面或Model处理。
 */
#pragma once

#include "shared/protocol/jsonlinecodec.h"

#include <QJsonObject>
#include <QObject>

class QTcpSocket;

// SocketClient把底层异步Socket转换为页面易用的Qt信号接口。
class SocketClient : public QObject
{
    Q_OBJECT

public:
    // 创建内部QTcpSocket并连接底层事件信号。
    explicit SocketClient(QObject *parent = nullptr);

    // 发起异步连接；结果通过connected或socketError信号返回。
    void connectToServer(const QString &host, quint16 port);
    // 请求Qt优雅断开当前连接。
    void disconnectFromServer();
    // 供页面决定当前是否允许发送请求。
    bool isConnected() const;

    // 发送标准请求并返回requestId；未连接时返回空字符串。
    QString sendRequest(const QString &type, const QString &sessionId,
                        const QJsonObject &payload,
                        const QString &requestId = {});

signals:
    // 连接状态与解析结果都通过信号通知UI，避免阻塞界面线程。
    void connected();
    void disconnected();
    void responseReceived(const QJsonObject &response);
    void protocolError(const QString &message);
    void socketError(const QString &message);

private slots:
    // 读取服务端字节并拆成一条或多条完整响应。
    void readAvailableData();

private:
    QTcpSocket *m_socket = nullptr;
    JsonLineCodec m_codec;
};
