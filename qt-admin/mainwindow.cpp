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
    buildLoginPage();
    connect(m_client, &AdminSocketClient::responseReceived, this, &MainWindow::handleResponse);
    connect(m_client, &AdminSocketClient::socketError, this, [this](const QString &message) {
        if (m_login) m_login->setEnabled(true);
        QMessageBox::warning(this, QStringLiteral("网络错误"), message);
    });
    connect(m_client, &AdminSocketClient::protocolError, this,
            [this](const QString &message) {
                QMessageBox::warning(this, QStringLiteral("协议错误"), message);
            });
    connect(m_client, &AdminSocketClient::requestTimedOut, this,
            [this](const QString &requestId, const QString &type) {
                m_requestTypes.remove(requestId); m_stationDetailRequests.remove(requestId);
                finishAction(type);
                QMessageBox::warning(this, QStringLiteral("请求超时"),
                                     QStringLiteral("%1 请求未及时响应").arg(type));
            });
    connect(m_client, &AdminSocketClient::requestFailed, this,
            [this](const QString &requestId, const QString &type,
                   const QString &message) {
                m_requestTypes.remove(requestId); m_stationDetailRequests.remove(requestId);
                finishAction(type);
                QMessageBox::warning(this, QStringLiteral("请求失败"),
                    QStringLiteral("%1：%2").arg(type, message));
            });
    connect(m_client, &AdminSocketClient::disconnected, this, [this]() {
        if (!m_sessionId.isEmpty()) {
            m_sessionId.clear();
            if (m_dashboardTimer) m_dashboardTimer->stop();
            QMessageBox::warning(this, QStringLiteral("连接已断开"),
                                 QStringLiteral("与服务端的连接已断开，请重新登录。"));
            buildLoginPage();
        }
    });
}

void MainWindow::buildLoginPage()
{
    if (m_dashboardTimer) { m_dashboardTimer->stop(); m_dashboardTimer->deleteLater(); m_dashboardTimer = nullptr; }
    m_tabs = nullptr; m_dashboard = nullptr; m_piles = nullptr; m_stations = nullptr; m_users = nullptr;
    auto *central = new QWidget(this); auto *layout = new QVBoxLayout(central);
    auto *form = new QFormLayout;
    m_host = new QLineEdit(m_lastHost, central);
    m_port = new QLineEdit(QString::number(m_lastPort), central);
    m_username = new QLineEdit(QStringLiteral("admin"), central);
    m_password = new QLineEdit(central); m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("服务端"), m_host); form->addRow(QStringLiteral("端口"), m_port);
    form->addRow(QStringLiteral("账号"), m_username); form->addRow(QStringLiteral("密码"), m_password);
    layout->addLayout(form); m_login = new QPushButton(QStringLiteral("登录"), central);
    m_status = new QLabel(QStringLiteral("请输入管理员账号和密码"), central);
    layout->addWidget(m_login); layout->addWidget(m_status); layout->addStretch(); setCentralWidget(central);
    connect(m_login, &QPushButton::clicked, this, &MainWindow::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &MainWindow::submitLogin);
    connect(m_client, &AdminSocketClient::connected, m_login, [this]() {
        const QString id = m_client->sendRequest(MessageTypes::AdminLogin, {}, {
            {QStringLiteral("username"), m_username->text().trimmed()},
            {QStringLiteral("password"), m_password->text()}});
        m_requestTypes.insert(id, MessageTypes::AdminLogin);
        m_status->setText(QStringLiteral("正在验证账号…"));
    });
}

void MainWindow::submitLogin()
{
    bool ok = false; const quint16 port = m_port->text().toUShort(&ok);
    if (!ok || m_host->text().trimmed().isEmpty()
        || m_username->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_status->setText(QStringLiteral("请填写有效的服务器、账号和密码")); return;
    }
    m_lastHost = m_host->text().trimmed(); m_lastPort = port;
    m_login->setEnabled(false); m_client->connectToServer(m_host->text().trimmed(), port);
}

QString MainWindow::send(const QString &type, const QJsonObject &payload)
{
    const QString id = m_client->sendRequest(type, m_sessionId, payload);
    if (!id.isEmpty()) m_requestTypes.insert(id, type); return id;
}

void MainWindow::handleResponse(const QJsonObject &response)
{
    const QString requestId = response.value(QStringLiteral("requestId")).toString();
    const QString type = m_requestTypes.take(requestId);
    const qint64 detailStationId = m_stationDetailRequests.take(requestId);
    finishAction(type);
    if (response.value(QStringLiteral("code")).toInt() != ErrorCodes::Success) { handleFailure(response, type); return; }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (type == MessageTypes::AdminLogin) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString(); buildManagementPages();
        refreshDashboard(); requestPileList(); requestStationList(); requestUserList();
    } else if (type == MessageTypes::AdminRevenueSummary) m_dashboard->setRevenueSummary(data);
    else if (type == MessageTypes::AdminRevenueTrend) m_dashboard->setRevenueTrend(data);
    else if (type == MessageTypes::AdminPileStatusSummary) m_dashboard->setPileStatusSummary(data);
    else if (type == MessageTypes::PredictionWarning) m_dashboard->setPredictionWarnings(data);
    else if (type == MessageTypes::AdminPileList && detailStationId > 0) m_stations->setStationPiles(detailStationId, data);
    else if (type == MessageTypes::AdminPileList) m_piles->setPiles(data);
    else if (type == MessageTypes::AdminStationList) m_stations->setStations(data);
    else if (type == MessageTypes::AdminUserList) m_users->setUsers(data);
    else if (type == MessageTypes::AdminPileRestart) {
        QMessageBox::information(this, QStringLiteral("操作成功"), QStringLiteral("远程重启指令已发送。"));
        QTimer::singleShot(1700, this, [this]() { requestPileList(); refreshDashboard(); });
    } else if (type == MessageTypes::AdminStationCreate) {
        QMessageBox::information(this, QStringLiteral("操作成功"), QStringLiteral("充电站创建成功。"));
        requestStationList(); requestPileList(); refreshDashboard();
    } else if (type == MessageTypes::AdminUserFreeze || type == MessageTypes::AdminUserUnfreeze) {
        QMessageBox::information(this, QStringLiteral("操作成功"), type == MessageTypes::AdminUserFreeze ? QStringLiteral("用户已冻结。") : QStringLiteral("用户已解冻。"));
        requestUserList();
    }
}

void MainWindow::buildManagementPages()
{
    m_tabs = new QTabWidget(this); m_dashboard = new DashboardPage(m_tabs);
    m_stations = new StationPage(m_tabs); m_piles = new PilePage(m_tabs); m_users = new UserPage(m_tabs);
    m_tabs->addTab(m_dashboard, QStringLiteral("运营概览")); m_tabs->addTab(m_stations, QStringLiteral("站点管理"));
    m_tabs->addTab(m_piles, QStringLiteral("电桩管理")); m_tabs->addTab(m_users, QStringLiteral("用户管理")); setCentralWidget(m_tabs);
    m_host = nullptr; m_port = nullptr; m_username = nullptr; m_password = nullptr; m_login = nullptr; m_status = nullptr;
    connect(m_dashboard, &DashboardPage::refreshRequested, this, &MainWindow::refreshDashboard);
    connect(m_dashboard, &DashboardPage::trendRequested, this, [this](int days) {
        m_trendDays = days; send(MessageTypes::AdminRevenueTrend, {{QStringLiteral("days"), days}});
    });
    connect(m_dashboard, &DashboardPage::warningRequested, this, [this](const QString &horizon) {
        m_warningHorizon = horizon;
        send(MessageTypes::PredictionWarning, {{QStringLiteral("horizon"), horizon}, {QStringLiteral("limit"), 20}});
    });
    connect(m_stations, &StationPage::refreshRequested, this, &MainWindow::requestStationList);
    connect(m_stations, &StationPage::stationPilesRequested, this, &MainWindow::requestStationPileDetails);
    connect(m_stations, &StationPage::createRequested, this, [this](const QJsonObject &payload) {
        m_stations->setCreateBusy(true);
        if (send(MessageTypes::AdminStationCreate, payload).isEmpty()) m_stations->setCreateBusy(false);
    });
    connect(m_piles, &PilePage::refreshRequested, this, [this]() { requestPileList(); });
    connect(m_piles, &PilePage::restartRequested, this, [this](qint64 id) {
        if (QMessageBox::question(this, QStringLiteral("确认重启"), QStringLiteral("确认远程重启该电桩吗？")) != QMessageBox::Yes) return;
        m_piles->setActionBusy(true);
        if (send(MessageTypes::AdminPileRestart, {{QStringLiteral("pileId"), id}}).isEmpty()) m_piles->setActionBusy(false);
    });
    connect(m_users, &UserPage::refreshRequested, this, &MainWindow::requestUserList);
    connect(m_users, &UserPage::searchRequested, this, [this](const QString &) { requestUserList(); });
    connect(m_users, &UserPage::statusChangeRequested, this, [this](qint64 id, bool freeze) {
        const QString action = freeze ? QStringLiteral("冻结") : QStringLiteral("解冻");
        if (QMessageBox::question(this, QStringLiteral("确认%1").arg(action), QStringLiteral("确认%1该用户吗？").arg(action)) != QMessageBox::Yes) return;
        m_users->setActionBusy(true);
        if (send(freeze ? MessageTypes::AdminUserFreeze : MessageTypes::AdminUserUnfreeze,
                 {{QStringLiteral("userId"), id}}).isEmpty()) m_users->setActionBusy(false);
    });
    m_dashboardTimer = new QTimer(this); m_dashboardTimer->setInterval(30000);
    connect(m_dashboardTimer, &QTimer::timeout, this, &MainWindow::refreshDashboard); m_dashboardTimer->start();
}

void MainWindow::refreshDashboard()
{
    send(MessageTypes::AdminRevenueSummary); send(MessageTypes::AdminRevenueTrend, {{QStringLiteral("days"), m_trendDays}});
    send(MessageTypes::AdminPileStatusSummary);
    send(MessageTypes::PredictionWarning, {{QStringLiteral("horizon"), m_warningHorizon}, {QStringLiteral("limit"), 20}});
}
void MainWindow::requestPileList(const QJsonObject &payload) { send(MessageTypes::AdminPileList, payload); }
void MainWindow::requestStationPileDetails(qint64 stationId)
{
    const QString id = send(MessageTypes::AdminPileList, {{QStringLiteral("stationId"), stationId}});
    if (!id.isEmpty()) m_stationDetailRequests.insert(id, stationId);
}
void MainWindow::requestStationList() { send(MessageTypes::AdminStationList); }
void MainWindow::requestUserList() { send(MessageTypes::AdminUserList, {{QStringLiteral("phoneKeyword"), m_users ? m_users->phoneKeyword() : QString()}}); }

void MainWindow::finishAction(const QString &type)
{
    if (type == MessageTypes::AdminPileRestart && m_piles) m_piles->setActionBusy(false);
    else if (type == MessageTypes::AdminStationCreate && m_stations) m_stations->setCreateBusy(false);
    else if ((type == MessageTypes::AdminUserFreeze || type == MessageTypes::AdminUserUnfreeze) && m_users) m_users->setActionBusy(false);
    else if (type == MessageTypes::AdminLogin && m_login) m_login->setEnabled(true);
}

void MainWindow::handleFailure(const QJsonObject &response, const QString &)
{
    const int code = response.value(QStringLiteral("code")).toInt();
    if (code == ErrorCodes::InvalidSession) {
        m_sessionId.clear(); if (m_dashboardTimer) m_dashboardTimer->stop();
        QMessageBox::warning(this, QStringLiteral("会话已失效"), QStringLiteral("请重新登录。"));
        buildLoginPage(); return;
    }
    QMessageBox::warning(this, QStringLiteral("操作失败"), QStringLiteral("错误码 %1：%2").arg(code).arg(response.value(QStringLiteral("message")).toString()));
}
