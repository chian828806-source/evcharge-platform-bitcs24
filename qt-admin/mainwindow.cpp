#include "mainwindow.h"

#include "network/adminsocketclient.h"
#include "shared/protocol/messagetypes.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_client(new AdminSocketClient(this))
{
    setWindowTitle(QStringLiteral("EVCharge 运营管理端"));
    resize(480, 360);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("管理员登录"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *form = new QFormLayout;
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_port = new QLineEdit(QStringLiteral("18080"), central);
    m_username = new QLineEdit(QStringLiteral("admin"), central);
    m_password = new QLineEdit(central);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("服务端"), m_host);
    form->addRow(QStringLiteral("端口"), m_port);
    form->addRow(QStringLiteral("账号"), m_username);
    form->addRow(QStringLiteral("密码"), m_password);
    layout->addLayout(form);
    m_login = new QPushButton(QStringLiteral("登录"), central);
    m_status = new QLabel(QStringLiteral("请输入管理员账号和密码"), central);
    layout->addWidget(m_login);
    layout->addWidget(m_status);
    layout->addStretch();
    setCentralWidget(central);

    connect(m_login, &QPushButton::clicked, this, &MainWindow::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &MainWindow::submitLogin);
    connect(m_client, &AdminSocketClient::connected, this, [this]() {
        m_loginRequestId = m_client->sendRequest(
            MessageTypes::AdminLogin, {}, {
                {QStringLiteral("username"), m_username->text().trimmed()},
                {QStringLiteral("password"), m_password->text()}
            });
        m_status->setText(QStringLiteral("正在验证账号…"));
    });
    connect(m_client, &AdminSocketClient::responseReceived,
            this, &MainWindow::handleResponse);
    connect(m_client, &AdminSocketClient::socketError, this,
            [this](const QString &message) {
                m_login->setEnabled(true);
                m_status->setText(QStringLiteral("连接失败：") + message);
            });
}

void MainWindow::submitLogin()
{
    bool portOk = false;
    const quint16 port = m_port->text().toUShort(&portOk);
    if (!portOk || m_username->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_status->setText(QStringLiteral("请填写有效的服务器、账号和密码"));
        return;
    }
    m_login->setEnabled(false);
    m_status->setText(QStringLiteral("正在连接服务端…"));
    m_client->connectToServer(m_host->text().trimmed(), port);
}

void MainWindow::handleResponse(const QJsonObject &response)
{
    const QString requestId = response.value(QStringLiteral("requestId")).toString();
    if (requestId == m_summaryRequestId) {
        if (response.value(QStringLiteral("code")).toInt() == 200) {
            showRevenueSummary(response.value(QStringLiteral("data")).toObject());
            m_trendRequestId = m_client->sendRequest(
                MessageTypes::AdminRevenueTrend, m_sessionId,
                {{QStringLiteral("days"), 7}});
        } else {
            m_status->setText(response.value(QStringLiteral("message")).toString());
        }
        return;
    }
    if (requestId == m_trendRequestId) {
        if (response.value(QStringLiteral("code")).toInt() == 200) {
            showRevenueTrend(response.value(QStringLiteral("data")).toObject());
            m_pileStatusRequestId = m_client->sendRequest(
                MessageTypes::AdminPileStatusSummary, m_sessionId, {});
        }
        return;
    }
    if (requestId == m_pileStatusRequestId) {
        if (response.value(QStringLiteral("code")).toInt() == 200) {
            showPileStatusSummary(response.value(QStringLiteral("data")).toObject());
        }
        return;
    }
    if (requestId != m_loginRequestId) {
        return;
    }
    m_login->setEnabled(true);
    if (response.value(QStringLiteral("code")).toInt() != 200) {
        m_status->setText(response.value(QStringLiteral("message")).toString());
        return;
    }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    const QJsonObject admin = data.value(QStringLiteral("admin")).toObject();
    m_status->setText(QStringLiteral("登录成功，欢迎 ")
                      + admin.value(QStringLiteral("displayName")).toString());
    m_sessionId = data.value(QStringLiteral("sessionId")).toString();
    m_summaryRequestId = m_client->sendRequest(
        MessageTypes::AdminRevenueSummary, m_sessionId, {});
}

void MainWindow::showPileStatusSummary(const QJsonObject &data)
{
    auto *series = new QPieSeries;
    const QJsonArray statuses = data.value(QStringLiteral("statuses")).toArray();
    for (const QJsonValue &value : statuses) {
        const QJsonObject item = value.toObject();
        const int count = item.value(QStringLiteral("count")).toInt();
        if (count > 0) {
            series->append(item.value(QStringLiteral("status")).toString()
                               + QStringLiteral(" %1").arg(count), count);
        }
    }
    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("电桩状态统计（总数 %1）")
                    .arg(data.value(QStringLiteral("total")).toInt()));
    auto *view = new QChartView(chart, centralWidget());
    view->setRenderHint(QPainter::Antialiasing);
    if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout())) {
        layout->insertWidget(layout->count() - 1, view, 1);
    }
}

void MainWindow::showRevenueTrend(const QJsonObject &data)
{
    auto *series = new QLineSeries;
    const QJsonArray points = data.value(QStringLiteral("points")).toArray();
    for (int index = 0; index < points.size(); ++index) {
        series->append(index, points.at(index).toObject()
                                 .value(QStringLiteral("revenueFen")).toDouble() / 100.0);
    }
    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("近 %1 日营收趋势（元）")
                    .arg(data.value(QStringLiteral("days")).toInt()));
    chart->createDefaultAxes();
    chart->legend()->hide();
    auto *view = new QChartView(chart, centralWidget());
    view->setRenderHint(QPainter::Antialiasing);
    if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout())) {
        layout->insertWidget(layout->count() - 1, view, 1);
    }
}

void MainWindow::showRevenueSummary(const QJsonObject &data)
{
    auto yuan = [](qint64 fen) {
        return QString::number(fen / 100.0, 'f', 2) + QStringLiteral(" 元");
    };
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    auto *title = new QLabel(QStringLiteral("核心营收指标"), panel);
    QFont font = title->font();
    font.setPointSize(20);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    layout->addWidget(new QLabel(QStringLiteral("今日营收：")
        + yuan(data.value(QStringLiteral("todayRevenueFen")).toInteger()), panel));
    layout->addWidget(new QLabel(QStringLiteral("本月营收：")
        + yuan(data.value(QStringLiteral("monthRevenueFen")).toInteger()), panel));
    layout->addWidget(new QLabel(QStringLiteral("总营收：")
        + yuan(data.value(QStringLiteral("totalRevenueFen")).toInteger()), panel));
    auto *sevenDays = new QPushButton(QStringLiteral("近 7 日趋势"), panel);
    auto *thirtyDays = new QPushButton(QStringLiteral("近 30 日趋势"), panel);
    layout->addWidget(sevenDays);
    layout->addWidget(thirtyDays);
    connect(sevenDays, &QPushButton::clicked, this, [this]() {
        m_trendRequestId = m_client->sendRequest(
            MessageTypes::AdminRevenueTrend, m_sessionId,
            {{QStringLiteral("days"), 7}});
    });
    connect(thirtyDays, &QPushButton::clicked, this, [this]() {
        m_trendRequestId = m_client->sendRequest(
            MessageTypes::AdminRevenueTrend, m_sessionId,
            {{QStringLiteral("days"), 30}});
    });
    layout->addStretch();
    setCentralWidget(panel);
}
