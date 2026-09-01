/*
 * 功能：实现Web大屏的WebSocket订阅入口和按topic广播。
 * 限制：只承载运营展示数据，不处理用户充电核心业务。
 */
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>

class QWebSocket;
class QWebSocketServer;

// 每个浏览器连接保存一组订阅主题。
class DashboardWebSocketServer : public QObject
{
    Q_OBJECT

public:
    // 创建非TLS的QWebSocketServer，尚不开始监听。
    explicit DashboardWebSocketServer(QObject *parent = nullptr);
    // 退出时关闭所有浏览器连接。
    ~DashboardWebSocketServer() override;

    // 监听WebSocket端口；HTTP升级路径在连接建立后验证。
    bool listen(quint16 port);
    // 关闭监听并清理当前订阅集合。
    void close();
    // 返回最近一次监听错误。
    QString errorString() const;
    // 向订阅指定topic的所有浏览器推送DASHBOARD_UPDATE。
    void publish(const QString &topic, const QJsonObject &data);

private slots:
    // 接收并验证一个新的浏览器连接。
    void acceptConnection();
    // 解析DASHBOARD_SUBSCRIBE并更新该连接的主题集合。
    void handleTextMessage(const QString &message);
    // 连接断开后移除订阅并释放QWebSocket。
    void removeConnection();

private:
    // 所有WebSocket响应都使用紧凑JSON文本帧。
    void sendJson(QWebSocket *socket, const QJsonObject &json);

    QWebSocketServer *m_server = nullptr;
    QHash<QWebSocket *, QSet<QString>> m_subscriptions;
};
