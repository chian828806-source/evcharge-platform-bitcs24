/*
 * 功能：独立业务服务端程序入口，装配Session、Dispatcher、TCP和WebSocket。
 * 边界：本进程不包含用户端或管理员端界面，业务Handler由Service模块注册。
 */
#include "network/dashboardwebsocketserver.h"
#include "network/messagedispatcher.h"
#include "network/sessionmanager.h"
#include "network/socketserver.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QTextStream>

int main(int argc, char *argv[])
{
    // QCoreApplication提供Qt事件循环，Socket信号依赖该循环工作。
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("evcharge-qt-server"));

    // 端口通过命令行提供默认值，方便多个开发者并行运行实例。
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("EVCharge Qt service"));
    parser.addHelpOption();
    parser.addOption({
        {QStringLiteral("t"), QStringLiteral("tcp-port")},
        QStringLiteral("TCP listen port"),
        QStringLiteral("port"),
        QStringLiteral("18080")
    });
    parser.addOption({
        {QStringLiteral("w"), QStringLiteral("websocket-port")},
        QStringLiteral("WebSocket listen port"),
        QStringLiteral("port"),
        QStringLiteral("18081")
    });
    parser.process(application);

    // 在监听前校验端口，避免toUShort失败后静默使用0。
    bool tcpPortValid = false;
    bool websocketPortValid = false;
    const quint16 tcpPort =
        parser.value(QStringLiteral("tcp-port")).toUShort(&tcpPortValid);
    const quint16 websocketPort =
        parser.value(QStringLiteral("websocket-port"))
            .toUShort(&websocketPortValid);
    if (!tcpPortValid || !websocketPortValid) {
        QTextStream(stderr) << "Invalid port value\n";
        return 2;
    }

    // 对象按依赖顺序创建，并存活到application退出。
    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);

    // 业务负责人通过 registerHandler() 注入 Service 调用。
    // 本网络外壳不伪造登录、订单或数据库结果。
    // 同一个TCP入口接收用户端和管理员端请求，角色由Session与路由权限区分。
    SocketServer socketServer(&dispatcher);
    if (!socketServer.listen(QHostAddress::Any, tcpPort)) {
        QTextStream(stderr) << "TCP listen failed: "
                            << socketServer.errorString() << '\n';
        return 1;
    }

    // WebSocket只服务大屏订阅和推送，使用独立端口。
    DashboardWebSocketServer dashboardServer;
    if (!dashboardServer.listen(websocketPort)) {
        QTextStream(stderr) << "WebSocket listen failed: "
                            << dashboardServer.errorString() << '\n';
        return 1;
    }

    // 输出实际监听信息，供启动脚本和人工联调确认。
    QTextStream(stdout) << "TCP listening on " << tcpPort << '\n'
                        << "WebSocket listening on " << websocketPort
                        << " path /dashboard\n";
    return application.exec();
}
