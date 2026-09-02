/*
 * 功能：封装QTcpServer监听，并为每个新连接创建ClientSession。
 * 边界：只管理连接，不读取业务payload，也不直接访问Service。
 */
#pragma once

#include <QObject>
#include <QHostAddress>

class MessageDispatcher;
class QTcpServer;

// SocketServer是独立业务服务端的TCP入口，由服务端main或启动器持有。
class SocketServer : public QObject
{
    Q_OBJECT

public:
    // dispatcher由上层持有，供所有连接共享同一套路由。
    explicit SocketServer(MessageDispatcher *dispatcher,
                          QObject *parent = nullptr);

    // 在指定地址和端口开始监听，失败时可通过errorString取原因。
    bool listen(const QHostAddress &address, quint16 port);
    // 停止接受新连接，已有连接由各自ClientSession管理。
    void close();
    // 返回最近一次QTcpServer错误的可读文本。
    QString errorString() const;

private slots:
    // 取出QTcpServer队列中的全部等待连接。
    void acceptConnections();

private:
    QTcpServer *m_server = nullptr;
    MessageDispatcher *m_dispatcher = nullptr;
};
