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
#include <QTableWidget>
#include <QHeaderView>
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
            m_pileListRequestId = m_client->sendRequest(
                MessageTypes::AdminPileList, m_sessionId, {});
        }
        return;
    }
    if (requestId == m_pileListRequestId) {
        if (response.value(QStringLiteral("code")).toInt() == 200) {
            showPileList(response.value(QStringLiteral("data")).toObject());
            m_stationListRequestId = m_client->sendRequest(
                MessageTypes::AdminStationList, m_sessionId, {});
        }
        return;
    }
    if (requestId == m_stationListRequestId) {
        if (response.value(QStringLiteral("code")).toInt() == 200) {
            showStationList(response.value(QStringLiteral("data")).toObject());
        }
        return;
    }
    if (requestId == m_pileRestartRequestId) {
        m_pileListRequestId = m_client->sendRequest(
            MessageTypes::AdminPileList, m_sessionId, {});
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

void MainWindow::showStationList(const QJsonObject &data)
{
    const QJsonArray stations = data.value(QStringLiteral("stations")).toArray();
    auto *table = new QTableWidget(stations.size(), 8, centralWidget());
    table->setHorizontalHeaderLabels({QStringLiteral("站点编号"), QStringLiteral("名称"),
        QStringLiteral("地址"), QStringLiteral("经度"), QStringLiteral("纬度"),
        QStringLiteral("电桩数"), QStringLiteral("在线率"), QStringLiteral("操作")});
    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations.at(row).toObject();
        const QStringList values = {station.value(QStringLiteral("stationNo")).toString(),
            station.value(QStringLiteral("name")).toString(),
            station.value(QStringLiteral("address")).toString(),
            QString::number(station.value(QStringLiteral("longitude")).toDouble(), 'f', 6),
            QString::number(station.value(QStringLiteral("latitude")).toDouble(), 'f', 6),
            QString::number(station.value(QStringLiteral("pileCount")).toInt()),
            QString::number(station.value(QStringLiteral("onlineRate")).toDouble() * 100, 'f', 1) + '%'};
        for (int column = 0; column < values.size(); ++column) {
            table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
        auto *viewPiles = new QPushButton(QStringLiteral("查看站内电桩"), table);
        const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
        connect(viewPiles, &QPushButton::clicked, this, [this, stationId]() {
            m_pileListRequestId = m_client->sendRequest(
                MessageTypes::AdminPileList, m_sessionId,
                {{QStringLiteral("stationId"), stationId}});
        });
        table->setCellWidget(row, 7, viewPiles);
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout())) {
        layout->insertWidget(layout->count() - 1, table, 2);
    }
}

void MainWindow::showPileList(const QJsonObject &data)
{
    const QJsonArray piles = data.value(QStringLiteral("piles")).toArray();
    auto *table = new QTableWidget(piles.size(), 8, centralWidget());
    table->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("站点"),
        QStringLiteral("类型"), QStringLiteral("功率/kW"), QStringLiteral("状态"),
        QStringLiteral("累计次数"), QStringLiteral("累计分钟"), QStringLiteral("操作")});
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QStringList values = {pile.value(QStringLiteral("pileNo")).toString(),
            pile.value(QStringLiteral("stationName")).toString(),
            pile.value(QStringLiteral("type")).toString(),
            QString::number(pile.value(QStringLiteral("powerKw")).toDouble()),
            pile.value(QStringLiteral("status")).toString(),
            QString::number(pile.value(QStringLiteral("totalChargeCount")).toInt()),
            QString::number(pile.value(QStringLiteral("totalChargeMinutes")).toInt())};
        for (int column = 0; column < values.size(); ++column) {
            table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
        auto *restart = new QPushButton(QStringLiteral("远程重启"), table);
        const qint64 pileId = pile.value(QStringLiteral("pileId")).toInteger();
        const QString status = pile.value(QStringLiteral("status")).toString();
        restart->setEnabled(status != QStringLiteral("RESERVED")
                            && status != QStringLiteral("CHARGING")
                            && status != QStringLiteral("RESTARTING"));
        connect(restart, &QPushButton::clicked, this, [this, pileId]() {
            m_pileRestartRequestId = m_client->sendRequest(
                MessageTypes::AdminPileRestart, m_sessionId,
                {{QStringLiteral("pileId"), pileId}});
        });
        table->setCellWidget(row, 7, restart);
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout())) {
        layout->insertWidget(layout->count() - 1, table, 2);
    }
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
