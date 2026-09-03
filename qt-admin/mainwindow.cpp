#include "mainwindow.h"
#include "network/adminsocketclient.h"
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/messagetypes.h"
#include "ui/adminpages.h"
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_client(new AdminSocketClient(this))
{
    setWindowTitle(QStringLiteral("EVCharge 运营管理端"));
    setMinimumSize(980, 680); resize(1180, 780);
    auto *central = new QWidget(this); central->setObjectName(QStringLiteral("loginRoot"));
    auto *outer = new QHBoxLayout(central); outer->setContentsMargins(80, 50, 80, 50);
    auto *card = new QFrame(central); card->setObjectName(QStringLiteral("loginCard")); card->setMaximumWidth(520);
    auto *layout = new QVBoxLayout(card); layout->setContentsMargins(42, 38, 42, 38); layout->setSpacing(14);
    auto *brand = new QLabel(QStringLiteral("EVCHARGE · OPERATIONS"), card); brand->setProperty("role", "brand");
    auto *title = new QLabel(QStringLiteral("运营管理平台"), card); title->setProperty("role", "hero");
    auto *subtitle = new QLabel(QStringLiteral("登录后查看充电网络运营状态与设备信息"), card); subtitle->setProperty("role", "subtitle");
    layout->addWidget(brand); layout->addWidget(title); layout->addWidget(subtitle); layout->addSpacing(12);
    auto *form = new QFormLayout; form->setSpacing(12);
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_port = new QLineEdit(QStringLiteral("18080"), central);
    m_username = new QLineEdit(QStringLiteral("admin"), central);
    m_password = new QLineEdit(central); m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("账号"), m_username); form->addRow(QStringLiteral("密码"), m_password);
    layout->addLayout(form);
    auto *settingsButton = new QPushButton(QStringLiteral("连接设置 ▾"), card);
    settingsButton->setProperty("kind", "secondary");
    auto *settings = new QWidget(card); auto *settingsForm = new QFormLayout(settings);
    settingsForm->setContentsMargins(0, 0, 0, 0);
    settingsForm->addRow(QStringLiteral("服务端"), m_host); settingsForm->addRow(QStringLiteral("端口"), m_port);
    settings->setVisible(false); layout->addWidget(settingsButton); layout->addWidget(settings);
    m_login = new QPushButton(QStringLiteral("登录管理端"), central); m_login->setMinimumHeight(44);
    auto *preview = new QPushButton(QStringLiteral("预览管理界面（Mock）"), card);
    preview->setProperty("kind", "secondary");
    m_status = new QLabel(QStringLiteral("请输入管理员账号和密码"), central);
    m_status->setProperty("role", "caption");
    layout->addWidget(m_login); layout->addWidget(preview); layout->addWidget(m_status); layout->addStretch();
    outer->addStretch(); outer->addWidget(card); outer->addStretch(); setCentralWidget(central);
    connect(settingsButton, &QPushButton::clicked, settings, [settings, settingsButton]() {
        settings->setVisible(!settings->isVisible());
        settingsButton->setText(settings->isVisible() ? QStringLiteral("连接设置 ▴")
                                                       : QStringLiteral("连接设置 ▾"));
    });
    connect(preview, &QPushButton::clicked, this, [this]() {
        buildManagementPages();
        m_dashboard->setRevenueSummary({{QStringLiteral("todayRevenueFen"), 128600},
                                        {QStringLiteral("monthRevenueFen"), 2864200},
                                        {QStringLiteral("totalRevenueFen"), 18642000}});
        m_dashboard->setRevenueTrend({{QStringLiteral("days"), 7},
            {QStringLiteral("points"), QJsonArray{
                QJsonObject{{"date", "09-01"}, {"revenueFen", 86400}, {"energyKwh", 720.0}, {"orderCount", 42}},
                QJsonObject{{"date", "09-02"}, {"revenueFen", 103200}, {"energyKwh", 860.0}, {"orderCount", 51}},
                QJsonObject{{"date", "09-03"}, {"revenueFen", 128600}, {"energyKwh", 1018.0}, {"orderCount", 63}}
            }}});
        m_dashboard->setPileStatusSummary({{QStringLiteral("total"), 24},
            {QStringLiteral("statuses"), QJsonArray{
                QJsonObject{{"status", "AVAILABLE"}, {"count", 12}, {"ratio", 0.50}},
                QJsonObject{{"status", "CHARGING"}, {"count", 7}, {"ratio", 0.29}},
                QJsonObject{{"status", "RESERVED"}, {"count", 3}, {"ratio", 0.13}},
                QJsonObject{{"status", "FAULT"}, {"count", 2}, {"ratio", 0.08}}
            }}});
        m_dashboard->setWarnings({{QStringLiteral("predictions"), QJsonArray{
            QJsonObject{{"stationName", "万达广场充电中心"}, {"predictionTime", "2026-09-03 17:00"}, {"predictedLoad", 0.91}, {"peakLevel", "HIGH"}},
            QJsonObject{{"stationName", "软件园智慧充电站"}, {"predictionTime", "2026-09-03 18:00"}, {"predictedLoad", 0.78}, {"peakLevel", "MEDIUM"}}
        }}});
        m_piles->setPiles({{QStringLiteral("piles"), QJsonArray{
            QJsonObject{{"pileId", 1}, {"pileNo", "P01"}, {"stationName", "软件园智慧充电站"}, {"type", "FAST"}, {"powerKw", 60.0}, {"status", "AVAILABLE"}, {"totalChargeCount", 126}, {"totalChargeMinutes", 3820}},
            QJsonObject{{"pileId", 2}, {"pileNo", "P02"}, {"stationName", "软件园智慧充电站"}, {"type", "FAST"}, {"powerKw", 60.0}, {"status", "CHARGING"}, {"totalChargeCount", 98}, {"totalChargeMinutes", 2914}},
            QJsonObject{{"pileId", 7}, {"pileNo", "A07"}, {"stationName", "万达广场充电中心"}, {"type", "SLOW"}, {"powerKw", 7.0}, {"status", "FAULT"}, {"totalChargeCount", 57}, {"totalChargeMinutes", 4860}}
        }}});
        m_stations->setStations({{QStringLiteral("stations"), QJsonArray{
            QJsonObject{{"stationId", 1}, {"stationNo", "ST001"}, {"name", "软件园智慧充电站"}, {"address", "软件园路 8 号"}, {"longitude", 121.538}, {"latitude", 38.889}, {"pileCount", 4}, {"onlineRate", 1.0}},
            QJsonObject{{"stationId", 2}, {"stationNo", "ST002"}, {"name", "万达广场充电中心"}, {"address", "虹韵路 6 号"}, {"longitude", 121.572}, {"latitude", 38.918}, {"pileCount", 12}, {"onlineRate", 0.92}},
            QJsonObject{{"stationId", 3}, {"stationNo", "ST003"}, {"name", "星海绿色能源站"}, {"address", "中山路 608 号"}, {"longitude", 121.584}, {"latitude", 38.881}, {"pileCount", 8}, {"onlineRate", 0.88}}
        }}});
        m_stations->setPileDetails(QJsonArray{
            QJsonObject{{"pileNo", "P01"}, {"powerKw", 60.0}, {"status", "AVAILABLE"}},
            QJsonObject{{"pileNo", "P02"}, {"powerKw", 60.0}, {"status", "CHARGING"}},
            QJsonObject{{"pileNo", "P03"}, {"powerKw", 7.0}, {"status", "RESERVED"}},
            QJsonObject{{"pileNo", "P04"}, {"powerKw", 7.0}, {"status", "FAULT"}}
        });
        m_users->setUsers({{QStringLiteral("users"), QJsonArray{
            QJsonObject{{"userId", 1}, {"phone", "13800000001"}, {"nickname", "海风"}, {"balanceFen", 12860}, {"createdAt", "2026-08-12 09:30"}, {"status", "NORMAL"}},
            QJsonObject{{"userId", 2}, {"phone", "13800000002"}, {"nickname", "满电出发"}, {"balanceFen", 5200}, {"createdAt", "2026-08-18 14:12"}, {"status", "NORMAL"}},
            QJsonObject{{"userId", 4}, {"phone", "13800000004"}, {"nickname", "测试用户"}, {"balanceFen", 800}, {"createdAt", "2026-08-26 11:06"}, {"status", "FROZEN"}}
        }}});
    });
    connect(m_login, &QPushButton::clicked, this, &MainWindow::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &MainWindow::submitLogin);
    connect(m_client, &AdminSocketClient::connected, this, [this]() {
        const QString id = m_client->sendRequest(MessageTypes::AdminLogin, {}, {
            {QStringLiteral("username"), m_username->text().trimmed()},
            {QStringLiteral("password"), m_password->text()}});
        m_requestTypes.insert(id, MessageTypes::AdminLogin);
        m_status->setText(QStringLiteral("正在验证账号…"));
    });
    connect(m_client, &AdminSocketClient::responseReceived, this, &MainWindow::handleResponse);
    connect(m_client, &AdminSocketClient::socketError, this, [this](const QString &message) {
        m_login->setEnabled(true); QMessageBox::warning(this, QStringLiteral("网络错误"), message);
    });
    connect(m_client, &AdminSocketClient::protocolError, this,
            [this](const QString &message) {
                QMessageBox::warning(this, QStringLiteral("协议错误"), message);
            });
    connect(m_client, &AdminSocketClient::requestTimedOut, this,
            [this](const QString &requestId, const QString &type) {
                m_requestTypes.remove(requestId);
                QMessageBox::warning(this, QStringLiteral("请求超时"),
                                     QStringLiteral("%1 请求未及时响应").arg(type));
            });
    connect(m_client, &AdminSocketClient::requestFailed, this,
            [this](const QString &requestId, const QString &type,
                   const QString &message) {
                m_requestTypes.remove(requestId);
                QMessageBox::warning(this, QStringLiteral("请求失败"),
                    QStringLiteral("%1：%2").arg(type, message));
            });
    connect(m_client, &AdminSocketClient::disconnected, this, [this]() {
        if (!m_sessionId.isEmpty()) {
            m_sessionId.clear();
            if (m_dashboardTimer) m_dashboardTimer->stop();
            QMessageBox::warning(this, QStringLiteral("连接已断开"),
                                 QStringLiteral("与服务端的连接已断开。"));
        }
    });
}

void MainWindow::submitLogin()
{
    bool ok = false; const quint16 port = m_port->text().toUShort(&ok);
    if (!ok || m_username->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_status->setText(QStringLiteral("请填写有效的服务器、账号和密码")); return;
    }
    m_login->setEnabled(false); m_client->connectToServer(m_host->text().trimmed(), port);
}

QString MainWindow::send(const QString &type, const QJsonObject &payload)
{
    const QString id = m_client->sendRequest(type, m_sessionId, payload);
    if (!id.isEmpty()) {
        m_requestTypes.insert(id, type);
    }
    return id;
}

void MainWindow::handleResponse(const QJsonObject &response)
{
    const QString type = m_requestTypes.take(response.value(QStringLiteral("requestId")).toString());
    if (response.value(QStringLiteral("code")).toInt() != ErrorCodes::Success) { handleFailure(response); return; }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (type == MessageTypes::AdminLogin) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString(); buildManagementPages();
        refreshDashboard(); requestPileList(); requestStationList(); requestUserList();
    } else if (type == MessageTypes::AdminRevenueSummary) m_dashboard->setRevenueSummary(data);
    else if (type == MessageTypes::AdminRevenueTrend) m_dashboard->setRevenueTrend(data);
    else if (type == MessageTypes::AdminPileStatusSummary) m_dashboard->setPileStatusSummary(data);
    else if (type == MessageTypes::AdminPileList) m_piles->setPiles(data);
    else if (type == MessageTypes::AdminStationList) m_stations->setStations(data);
    else if (type == MessageTypes::AdminUserList) m_users->setUsers(data);
    else if (type == MessageTypes::AdminPileRestart)
        QTimer::singleShot(1700, this, [this]() { requestPileList(); refreshDashboard(); });
    else if (type == MessageTypes::AdminStationCreate) { requestStationList(); requestPileList(); refreshDashboard(); }
    else if (type == MessageTypes::AdminUserFreeze || type == MessageTypes::AdminUserUnfreeze) requestUserList();
}

void MainWindow::buildManagementPages()
{
    m_tabs = new QTabWidget(this); m_dashboard = new DashboardPage(m_tabs);
    m_stations = new StationPage(m_tabs); m_piles = new PilePage(m_tabs); m_users = new UserPage(m_tabs);
    m_tabs->addTab(m_dashboard, QStringLiteral("运营概览")); m_tabs->addTab(m_stations, QStringLiteral("站点管理"));
    m_tabs->addTab(m_piles, QStringLiteral("电桩管理")); m_tabs->addTab(m_users, QStringLiteral("用户管理")); setCentralWidget(m_tabs);
    connect(m_dashboard, &DashboardPage::refreshRequested, this, &MainWindow::refreshDashboard);
    connect(m_dashboard, &DashboardPage::trendRequested, this, [this](int days) { send(MessageTypes::AdminRevenueTrend, {{QStringLiteral("days"), days}}); });
    connect(m_stations, &StationPage::refreshRequested, this, &MainWindow::requestStationList);
    connect(m_stations, &StationPage::stationPilesRequested, this, [this](qint64 id) { requestPileList({{QStringLiteral("stationId"), id}}); m_tabs->setCurrentWidget(m_piles); });
    connect(m_stations, &StationPage::createRequested, this, [this](const QJsonObject &payload) { send(MessageTypes::AdminStationCreate, payload); });
    connect(m_piles, &PilePage::refreshRequested, this, [this]() { requestPileList(); });
    connect(m_piles, &PilePage::restartRequested, this, [this](qint64 id) { send(MessageTypes::AdminPileRestart, {{QStringLiteral("pileId"), id}}); });
    connect(m_users, &UserPage::refreshRequested, this, &MainWindow::requestUserList);
    connect(m_users, &UserPage::searchRequested, this, [this](const QString &) { requestUserList(); });
    connect(m_users, &UserPage::statusChangeRequested, this, [this](qint64 id, bool freeze) {
        send(freeze ? MessageTypes::AdminUserFreeze : MessageTypes::AdminUserUnfreeze,
             {{QStringLiteral("userId"), id}});
    });
    m_dashboardTimer = new QTimer(this); m_dashboardTimer->setInterval(30000);
    connect(m_dashboardTimer, &QTimer::timeout, this, &MainWindow::refreshDashboard);
    if (m_client->isConnected()) m_dashboardTimer->start();
}

void MainWindow::refreshDashboard()
{
    send(MessageTypes::AdminRevenueSummary); send(MessageTypes::AdminRevenueTrend, {{QStringLiteral("days"), 7}});
    send(MessageTypes::AdminPileStatusSummary);
}
void MainWindow::requestPileList(const QJsonObject &payload) { send(MessageTypes::AdminPileList, payload); }
void MainWindow::requestStationList() { send(MessageTypes::AdminStationList); }
void MainWindow::requestUserList() { send(MessageTypes::AdminUserList, {{QStringLiteral("phoneKeyword"), m_users ? m_users->phoneKeyword() : QString()}}); }

void MainWindow::handleFailure(const QJsonObject &response)
{
    const int code = response.value(QStringLiteral("code")).toInt();
    if (code == ErrorCodes::InvalidSession) {
        m_sessionId.clear(); if (m_dashboardTimer) m_dashboardTimer->stop();
        QMessageBox::warning(this, QStringLiteral("会话已失效"), QStringLiteral("请重新启动管理端并登录。"));
        setEnabled(false); return;
    }
    QMessageBox::warning(this, QStringLiteral("操作失败"), QStringLiteral("错误码 %1：%2").arg(code).arg(response.value(QStringLiteral("message")).toString()));
}
