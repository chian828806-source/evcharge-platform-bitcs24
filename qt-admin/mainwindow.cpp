#include "mainwindow.h"
#include "network/adminsocketclient.h"
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/messagetypes.h"
#include "ui/adminpages.h"
#include <QFormLayout>
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
    setWindowTitle(QStringLiteral("EVCharge 运营管理端")); resize(1100, 760);
    auto *central = new QWidget(this); auto *layout = new QVBoxLayout(central);
    auto *form = new QFormLayout;
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_port = new QLineEdit(QStringLiteral("18080"), central);
    m_username = new QLineEdit(QStringLiteral("admin"), central);
    m_password = new QLineEdit(central); m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("服务端"), m_host); form->addRow(QStringLiteral("端口"), m_port);
    form->addRow(QStringLiteral("账号"), m_username); form->addRow(QStringLiteral("密码"), m_password);
    layout->addLayout(form); m_login = new QPushButton(QStringLiteral("登录"), central);
    m_status = new QLabel(QStringLiteral("请输入管理员账号和密码"), central);
    layout->addWidget(m_login); layout->addWidget(m_status); layout->addStretch(); setCentralWidget(central);
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
    if (!id.isEmpty()) m_requestTypes.insert(id, type); return id;
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
    connect(m_dashboardTimer, &QTimer::timeout, this, &MainWindow::refreshDashboard); m_dashboardTimer->start();
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
