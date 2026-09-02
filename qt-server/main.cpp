/*
 * 功能：独立业务服务端程序入口，装配Session、Dispatcher、TCP和WebSocket。
 * 边界：本进程不包含用户端或管理员端界面，业务Handler由Service模块注册。
 */
#include "database/databasemanager.h"
#include "handlers/user/registeruserhandlers.h"
#include "handlers/user/registerstationhandlers.h"
#include "handlers/user/stationhandler.h"
#include "handlers/user/registerorderhandlers.h"
#include "handlers/user/orderhandler.h"
#include "handlers/user/userhandler.h"
#include "network/dashboardwebsocketserver.h"
#include "network/messagedispatcher.h"
#include "network/sessionmanager.h"
#include "network/socketserver.h"
#include "repositories/userrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/orderrepository.h"
#include "services/user/orderservice.h"
#include "services/user/stationservice.h"
#include "services/user/userservice.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
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
    parser.addOption({
        {QStringLiteral("d"), QStringLiteral("database")},
        QStringLiteral("SQLite database file; initialize it with database/schema.sql first"),
        QStringLiteral("path"),
        QStringLiteral("database/evcharge.db")
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

    // 启动前验证SQLite连接和user表，避免服务监听成功后才暴露配置错误。
    DatabaseManager databaseManager(parser.value(QStringLiteral("database")));
    QSqlDatabase database;
    QString databaseError;
    if (!databaseManager.database(&database, &databaseError)) {
        QTextStream(stderr) << "SQLite open failed: " << databaseError << '\n';
        return 3;
    }
    QSqlQuery schemaCheck(database);
    if (!schemaCheck.exec(QStringLiteral("SELECT 1 FROM user LIMIT 1"))) {
        QTextStream(stderr)
            << "SQLite schema is unavailable; initialize database/schema.sql first: "
            << schemaCheck.lastError().text() << '\n';
        return 3;
    }

    // 用户模块：Repository负责SQL，Service负责规则，Handler负责Socket映射。
    UserRepository userRepository;
    UserService userService(&databaseManager, &userRepository);
    UserHandler userHandler(&userService, &sessions);
    registerUserHandlers(&dispatcher, &userHandler);

    StationRepository stationRepository;
    StationService stationService(&databaseManager, &stationRepository);
    StationHandler stationHandler(&stationService);
    registerStationHandlers(&dispatcher, &stationHandler);

    OrderRepository orderRepository;
    OrderService orderService(&databaseManager, &userRepository, &orderRepository);
    OrderHandler orderHandler(&orderService);
    registerOrderHandlers(&dispatcher, &orderHandler);

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
