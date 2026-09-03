#include "adminpages.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QtCharts/QCategoryAxis>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>

namespace {
QPushButton *button(const QString &text, QWidget *parent)
{
    return new QPushButton(text, parent);
}

void replaceWidget(QVBoxLayout *layout, QWidget **current, QWidget *replacement)
{
    if (*current) {
        layout->removeWidget(*current);
        (*current)->deleteLater();
    }
    *current = replacement;
    layout->addWidget(replacement, 1);
}
}

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    auto *refresh = button(QStringLiteral("刷新 Dashboard"), this);
    auto *seven = button(QStringLiteral("近 7 日"), this);
    auto *thirty = button(QStringLiteral("近 30 日"), this);
    auto *warning1h = button(QStringLiteral("未来 1 小时预警"), this);
    auto *warning6h = button(QStringLiteral("未来 6 小时预警"), this);
    auto *warning24h = button(QStringLiteral("未来 24 小时预警"), this);
    m_today = new QLabel(this);
    m_month = new QLabel(this);
    m_total = new QLabel(this);
    root->addWidget(refresh);
    root->addWidget(seven);
    root->addWidget(thirty);
    root->addWidget(warning1h);
    root->addWidget(warning6h);
    root->addWidget(warning24h);
    root->addWidget(m_today);
    root->addWidget(m_month);
    root->addWidget(m_total);
    m_chartLayout = new QVBoxLayout;
    root->addLayout(m_chartLayout, 1);
    m_trendDetails = new QTableWidget(this);
    m_trendDetails->setMaximumHeight(180);
    m_warnings = new QTableWidget(this);
    m_warnings->setMaximumHeight(180);
    root->addWidget(new QLabel(QStringLiteral("趋势明细（日期 / 营收 / 电量 / 订单数）"), this));
    root->addWidget(m_trendDetails);
    root->addWidget(new QLabel(QStringLiteral("未来 1 小时高负荷预警"), this));
    root->addWidget(m_warnings);
    connect(refresh, &QPushButton::clicked, this, &DashboardPage::refreshRequested);
    connect(seven, &QPushButton::clicked, this, [this]() { emit trendRequested(7); });
    connect(thirty, &QPushButton::clicked, this, [this]() { emit trendRequested(30); });
    connect(warning1h, &QPushButton::clicked, this, [this]() { emit warningRequested(QStringLiteral("1h")); });
    connect(warning6h, &QPushButton::clicked, this, [this]() { emit warningRequested(QStringLiteral("6h")); });
    connect(warning24h, &QPushButton::clicked, this, [this]() { emit warningRequested(QStringLiteral("24h")); });
}

void DashboardPage::setRevenueSummary(const QJsonObject &data)
{
    auto yuan = [](qint64 fen) { return QString::number(fen / 100.0, 'f', 2); };
    m_today->setText(QStringLiteral("今日营收：%1 元").arg(yuan(data.value(QStringLiteral("todayRevenueFen")).toInteger())));
    m_month->setText(QStringLiteral("本月营收：%1 元").arg(yuan(data.value(QStringLiteral("monthRevenueFen")).toInteger())));
    m_total->setText(QStringLiteral("累计营收：%1 元").arg(yuan(data.value(QStringLiteral("totalRevenueFen")).toInteger())));
}

void DashboardPage::setRevenueTrend(const QJsonObject &data)
{
    auto *revenueSeries = new QLineSeries;
    auto *energySeries = new QLineSeries;
    auto *orderSeries = new QLineSeries;
    revenueSeries->setName(QStringLiteral("营收（元）"));
    energySeries->setName(QStringLiteral("充电量（kWh）"));
    orderSeries->setName(QStringLiteral("订单数"));
    const QJsonArray points = data.value(QStringLiteral("points")).toArray();
    auto *axisX = new QCategoryAxis;
    axisX->setLabelsAngle(-45);
    m_trendDetails->clear();
    m_trendDetails->setRowCount(points.size());
    m_trendDetails->setColumnCount(4);
    m_trendDetails->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("营收（元）"), QStringLiteral("充电量（kWh）"), QStringLiteral("订单数")});
    for (int i = 0; i < points.size(); ++i) {
        const QJsonObject point = points.at(i).toObject();
        const QString date = point.value(QStringLiteral("date")).toString();
        const double revenue = point.value(QStringLiteral("revenueFen")).toDouble() / 100.0;
        const double energy = point.value(QStringLiteral("energyKwh")).toDouble();
        const int orders = point.value(QStringLiteral("orderCount")).toInt();
        revenueSeries->append(i, revenue);
        energySeries->append(i, energy);
        orderSeries->append(i, orders);
        axisX->append(date.mid(5), i + 0.5);
        const QStringList values = {date, QString::number(revenue, 'f', 2), QString::number(energy, 'f', 2), QString::number(orders)};
        for (int column = 0; column < values.size(); ++column)
            m_trendDetails->setItem(i, column, new QTableWidgetItem(values.at(column)));
    }
    auto *chart = new QChart;
    chart->addSeries(revenueSeries);
    chart->addSeries(energySeries);
    chart->addSeries(orderSeries);
    chart->addAxis(axisX, Qt::AlignBottom);
    for (QLineSeries *series : {revenueSeries, energySeries, orderSeries})
        series->attachAxis(axisX);
    auto *axisY = new QValueAxis;
    axisY->setMin(0);
    chart->addAxis(axisY, Qt::AlignLeft);
    for (QLineSeries *series : {revenueSeries, energySeries, orderSeries})
        series->attachAxis(axisY);
    chart->setTitle(QStringLiteral("近 %1 日营收趋势").arg(data.value(QStringLiteral("days")).toInt()));
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    replaceWidget(m_chartLayout, &m_trendView, view);
    m_trendDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void DashboardPage::setPileStatusSummary(const QJsonObject &data)
{
    auto *series = new QPieSeries;
    for (const QJsonValue &value : data.value(QStringLiteral("statuses")).toArray()) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("count")).toInt() > 0) {
            series->append(item.value(QStringLiteral("status")).toString(),
                           item.value(QStringLiteral("count")).toInt());
        }
    }
    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("电桩状态（总数 %1）").arg(data.value(QStringLiteral("total")).toInt()));
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    replaceWidget(m_chartLayout, &m_statusView, view);
}

void DashboardPage::setPredictionWarnings(const QJsonObject &data)
{
    const QJsonArray warnings = data.value(QStringLiteral("predictions")).toArray();
    m_warnings->clear();
    m_warnings->setRowCount(warnings.size());
    m_warnings->setColumnCount(6);
    m_warnings->setHorizontalHeaderLabels({QStringLiteral("站点"), QStringLiteral("预测时间"), QStringLiteral("范围"), QStringLiteral("负荷率"), QStringLiteral("预计空闲"), QStringLiteral("峰值等级")});
    for (int row = 0; row < warnings.size(); ++row) {
        const QJsonObject item = warnings.at(row).toObject();
        const QStringList values = {
            item.value(QStringLiteral("stationName")).toString(),
            item.value(QStringLiteral("predictionTime")).toString(),
            item.value(QStringLiteral("horizon")).toString(),
            QString::number(item.value(QStringLiteral("predictedLoad")).toDouble() * 100.0, 'f', 1) + '%',
            QString::number(item.value(QStringLiteral("predictedAvailableCount")).toInt()),
            item.value(QStringLiteral("peakLevel")).toString()};
        for (int column = 0; column < values.size(); ++column)
            m_warnings->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    m_warnings->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

PilePage::PilePage(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this);
    auto *refresh = button(QStringLiteral("刷新电桩"), this);
    root->addWidget(refresh);
    root->addWidget(m_table, 1);
    connect(refresh, &QPushButton::clicked, this, &PilePage::refreshRequested);
}

void PilePage::setPiles(const QJsonObject &data)
{
    const QJsonArray piles = data.value(QStringLiteral("piles")).toArray();
    m_table->clear(); m_table->setRowCount(piles.size()); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("站点"), QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"), QStringLiteral("次数"), QStringLiteral("分钟"), QStringLiteral("操作")});
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QStringList values = {pile.value(QStringLiteral("pileNo")).toString(), pile.value(QStringLiteral("stationName")).toString(), pile.value(QStringLiteral("type")).toString(), QString::number(pile.value(QStringLiteral("powerKw")).toDouble()), pile.value(QStringLiteral("status")).toString(), QString::number(pile.value(QStringLiteral("totalChargeCount")).toInt()), QString::number(pile.value(QStringLiteral("totalChargeMinutes")).toInt())};
        for (int column = 0; column < values.size(); ++column) m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        auto *restart = button(QStringLiteral("远程重启"), m_table);
        const QString status = pile.value(QStringLiteral("status")).toString();
        restart->setEnabled(!m_actionBusy && status != QStringLiteral("RESERVED") && status != QStringLiteral("CHARGING") && status != QStringLiteral("RESTARTING"));
        const qint64 pileId = pile.value(QStringLiteral("pileId")).toInteger();
        connect(restart, &QPushButton::clicked, this, [this, pileId]() { emit restartRequested(pileId); });
        m_table->setCellWidget(row, 7, restart);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void PilePage::setActionBusy(bool busy)
{
    m_actionBusy = busy;
    m_table->setEnabled(!busy);
}

StationPage::StationPage(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this);
    auto *refresh = button(QStringLiteral("刷新站点"), this);
    m_create = button(QStringLiteral("新增站点"), this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    auto *detail = new QWidget(splitter);
    auto *detailLayout = new QVBoxLayout(detail);
    m_detailTitle = new QLabel(QStringLiteral("请选择左侧站点查看电桩"), detail);
    m_detailTable = new QTableWidget(detail);
    detailLayout->addWidget(m_detailTitle);
    detailLayout->addWidget(m_detailTable, 1);
    splitter->addWidget(m_table);
    splitter->addWidget(detail);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(refresh); root->addWidget(m_create); root->addWidget(splitter, 1);
    connect(refresh, &QPushButton::clicked, this, &StationPage::refreshRequested);
    connect(m_create, &QPushButton::clicked, this, &StationPage::openCreateDialog);
}

void StationPage::setStationPiles(qint64 stationId, const QJsonObject &data)
{
    const QJsonArray piles = data.value(QStringLiteral("piles")).toArray();
    m_detailTitle->setText(QStringLiteral("站点 %1 的电桩（%2）").arg(stationId).arg(piles.size()));
    m_detailTable->clear();
    m_detailTable->setRowCount(piles.size());
    m_detailTable->setColumnCount(4);
    m_detailTable->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态")});
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QStringList values = {pile.value(QStringLiteral("pileNo")).toString(), pile.value(QStringLiteral("type")).toString(), QString::number(pile.value(QStringLiteral("powerKw")).toDouble()) + QStringLiteral(" kW"), pile.value(QStringLiteral("status")).toString()};
        for (int column = 0; column < values.size(); ++column)
            m_detailTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    m_detailTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void StationPage::setCreateBusy(bool busy)
{
    m_create->setEnabled(!busy);
}

void StationPage::setStations(const QJsonObject &data)
{
    const QJsonArray stations = data.value(QStringLiteral("stations")).toArray();
    m_table->clear(); m_table->setRowCount(stations.size()); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("名称"), QStringLiteral("地址"), QStringLiteral("经度"), QStringLiteral("纬度"), QStringLiteral("电桩"), QStringLiteral("在线率"), QStringLiteral("操作")});
    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations.at(row).toObject();
        const QStringList values = {station.value(QStringLiteral("stationNo")).toString(), station.value(QStringLiteral("name")).toString(), station.value(QStringLiteral("address")).toString(), QString::number(station.value(QStringLiteral("longitude")).toDouble(), 'f', 6), QString::number(station.value(QStringLiteral("latitude")).toDouble(), 'f', 6), QString::number(station.value(QStringLiteral("pileCount")).toInt()), QString::number(station.value(QStringLiteral("onlineRate")).toDouble() * 100, 'f', 1) + '%'};
        for (int column = 0; column < values.size(); ++column) m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        auto *view = button(QStringLiteral("查看电桩"), m_table);
        const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
        connect(view, &QPushButton::clicked, this, [this, stationId]() { emit stationPilesRequested(stationId); });
        m_table->setCellWidget(row, 7, view);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void StationPage::openCreateDialog()
{
    QDialog dialog(this); QFormLayout form(&dialog); QLineEdit name, address;
    QDoubleSpinBox longitude, latitude, price; QSpinBox count;
    longitude.setRange(-180, 180); latitude.setRange(-90, 90); count.setRange(1, 100); count.setValue(4);
    price.setRange(0.01, 100.0); price.setDecimals(2); price.setValue(1.20); price.setSuffix(QStringLiteral(" 元/度"));
    form.addRow(QStringLiteral("站名"), &name); form.addRow(QStringLiteral("地址"), &address); form.addRow(QStringLiteral("经度"), &longitude); form.addRow(QStringLiteral("纬度"), &latitude); form.addRow(QStringLiteral("电桩数"), &count); form.addRow(QStringLiteral("充电单价"), &price);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) emit createRequested({{QStringLiteral("name"), name.text().trimmed()}, {QStringLiteral("address"), address.text().trimmed()}, {QStringLiteral("longitude"), longitude.value()}, {QStringLiteral("latitude"), latitude.value()}, {QStringLiteral("pileCount"), count.value()}, {QStringLiteral("priceFenPerKwh"), qRound64(price.value() * 100.0)}});
}

UserPage::UserPage(QWidget *parent) : QWidget(parent), m_search(new QLineEdit(this)), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this); auto *refresh = button(QStringLiteral("刷新用户"), this);
    m_search->setPlaceholderText(QStringLiteral("手机号关键字，回车查询"));
    root->addWidget(refresh); root->addWidget(m_search); root->addWidget(m_table, 1);
    connect(refresh, &QPushButton::clicked, this, &UserPage::refreshRequested);
    connect(m_search, &QLineEdit::returnPressed, this, [this]() { emit searchRequested(phoneKeyword()); });
}

QString UserPage::phoneKeyword() const { return m_search->text().trimmed(); }

void UserPage::setUsers(const QJsonObject &data)
{
    const QJsonArray users = data.value(QStringLiteral("users")).toArray();
    m_table->clear(); m_table->setRowCount(users.size()); m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"), QStringLiteral("余额"), QStringLiteral("注册时间"), QStringLiteral("状态"), QStringLiteral("操作")});
    for (int row = 0; row < users.size(); ++row) {
        const QJsonObject user = users.at(row).toObject();
        const QStringList values = {QString::number(user.value(QStringLiteral("userId")).toInteger()), user.value(QStringLiteral("phone")).toString(), user.value(QStringLiteral("nickname")).toString(), QString::number(user.value(QStringLiteral("balanceFen")).toInteger() / 100.0, 'f', 2), user.value(QStringLiteral("createdAt")).toString(), user.value(QStringLiteral("status")).toString()};
        for (int column = 0; column < values.size(); ++column) m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        const bool frozen = user.value(QStringLiteral("status")).toString() == QStringLiteral("FROZEN");
        auto *change = button(frozen ? QStringLiteral("解冻") : QStringLiteral("冻结"), m_table);
        change->setEnabled(!m_actionBusy);
        const qint64 userId = user.value(QStringLiteral("userId")).toInteger();
        connect(change, &QPushButton::clicked, this, [this, userId, frozen]() { emit statusChangeRequested(userId, !frozen); });
        m_table->setCellWidget(row, 6, change);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void UserPage::setActionBusy(bool busy)
{
    m_actionBusy = busy;
    m_table->setEnabled(!busy);
}
