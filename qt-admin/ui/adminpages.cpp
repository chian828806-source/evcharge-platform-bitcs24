#include "adminpages.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>

namespace {
QPushButton *button(const QString &text, QWidget *parent)
{
    return new QPushButton(text, parent);
}

QLabel *label(const QString &text, const char *role = nullptr)
{
    auto *item = new QLabel(text);
    if (role) item->setProperty("role", role);
    return item;
}

QFrame *panel()
{
    auto *item = new QFrame;
    item->setObjectName(QStringLiteral("panel"));
    return item;
}

void prepareTable(QTableWidget *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
}

QString statusText(const QString &status)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("AVAILABLE"), QStringLiteral("空闲")},
        {QStringLiteral("RESERVED"), QStringLiteral("已预约")},
        {QStringLiteral("CHARGING"), QStringLiteral("充电中")},
        {QStringLiteral("FAULT"), QStringLiteral("故障")},
        {QStringLiteral("OFFLINE"), QStringLiteral("离线")},
        {QStringLiteral("RESTARTING"), QStringLiteral("重启中")},
        {QStringLiteral("NORMAL"), QStringLiteral("正常")},
        {QStringLiteral("FROZEN"), QStringLiteral("已冻结")},
        {QStringLiteral("HIGH"), QStringLiteral("高峰")},
        {QStringLiteral("MEDIUM"), QStringLiteral("中等")},
        {QStringLiteral("LOW"), QStringLiteral("低")}
    };
    return names.value(status, status);
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
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 24); root->setSpacing(16);
    auto *top = new QHBoxLayout;
    auto *heading = new QVBoxLayout; heading->addWidget(label(QStringLiteral("运营概览"), "pageTitle"));
    heading->addWidget(label(QStringLiteral("查看营收、设备状态和负荷预测"), "subtitle")); top->addLayout(heading); top->addStretch();
    auto *refresh = button(QStringLiteral("刷新 Dashboard"), this);
    auto *seven = button(QStringLiteral("近 7 日"), this);
    auto *thirty = button(QStringLiteral("近 30 日"), this);
    seven->setProperty("kind", "secondary"); thirty->setProperty("kind", "secondary");
    top->addWidget(refresh); root->addLayout(top);
    auto *metrics = new QHBoxLayout;
    const QStringList names{QStringLiteral("今日营收"), QStringLiteral("本月营收"), QStringLiteral("累计营收")};
    QLabel **values[]{&m_today, &m_month, &m_total};
    for (int i = 0; i < names.size(); ++i) { auto *card = panel(); auto *box = new QVBoxLayout(card);
        box->setContentsMargins(20, 16, 20, 16); box->addWidget(label(names.at(i), "metricTitle"));
        *values[i] = label(QStringLiteral("¥ --"), "metricValue"); box->addWidget(*values[i]); metrics->addWidget(card, 1); }
    root->addLayout(metrics);
    auto *period = new QHBoxLayout; period->addWidget(label(QStringLiteral("营收趋势"), "pageTitle")); period->addStretch(); period->addWidget(seven); period->addWidget(thirty); root->addLayout(period);
    m_chartLayout = new QVBoxLayout;
    auto *chartPanel = panel(); chartPanel->setLayout(m_chartLayout); m_trendView = label(QStringLiteral("等待趋势数据"), "caption"); m_chartLayout->addWidget(m_trendView); root->addWidget(chartPanel, 2);
    auto *bottom = new QHBoxLayout;
    auto *statusPanel = panel(); m_statusLayout = new QVBoxLayout(statusPanel); m_statusLayout->addWidget(label(QStringLiteral("电桩状态"), "pageTitle"));
    m_statusView = label(QStringLiteral("等待状态数据"), "caption"); m_statusLayout->addWidget(m_statusView); bottom->addWidget(statusPanel, 1);
    auto *warningPanel = panel(); auto *warningLayout = new QVBoxLayout(warningPanel); auto *warningTop = new QHBoxLayout;
    warningTop->addWidget(label(QStringLiteral("负荷预警"), "pageTitle")); warningTop->addStretch();
    for (const QString &text : {QStringLiteral("1h"), QStringLiteral("6h"), QStringLiteral("24h")}) {
        auto *b = button(text, warningPanel); b->setProperty("kind", "secondary"); warningTop->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, text]() { emit warningRequested(text); });
    }
    warningLayout->addLayout(warningTop); m_warningTable = new QTableWidget(0, 6, warningPanel);
    m_warningTable->setHorizontalHeaderLabels({QStringLiteral("站点"), QStringLiteral("预测时间"), QStringLiteral("范围"), QStringLiteral("负荷率"), QStringLiteral("预计空闲"), QStringLiteral("峰值")}); prepareTable(m_warningTable);
    warningLayout->addWidget(m_warningTable); bottom->addWidget(warningPanel, 1); root->addLayout(bottom, 2);
    connect(refresh, &QPushButton::clicked, this, &DashboardPage::refreshRequested);
    connect(seven, &QPushButton::clicked, this, [this]() { emit trendRequested(7); });
    connect(thirty, &QPushButton::clicked, this, [this]() { emit trendRequested(30); });
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
    for (int i = 0; i < points.size(); ++i) {
        const QJsonObject point = points.at(i).toObject();
        revenueSeries->append(i, point.value(QStringLiteral("revenueFen")).toDouble() / 100.0);
        energySeries->append(i, point.value(QStringLiteral("energyKwh")).toDouble());
        orderSeries->append(i, point.value(QStringLiteral("orderCount")).toInt());
        axisX->append(point.value(QStringLiteral("date")).toString(), i + 0.5);
    }
    auto *chart = new QChart;
    chart->addSeries(revenueSeries); chart->addSeries(energySeries); chart->addSeries(orderSeries);
    chart->addAxis(axisX, Qt::AlignBottom);
    auto *axisY = new QValueAxis; axisY->setMin(0); chart->addAxis(axisY, Qt::AlignLeft);
    for (QLineSeries *series : {revenueSeries, energySeries, orderSeries}) {
        series->attachAxis(axisX); series->attachAxis(axisY);
    }
    chart->setTitle(QStringLiteral("近 %1 日营收趋势").arg(data.value(QStringLiteral("days")).toInt()));
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    replaceWidget(m_chartLayout, &m_trendView, view);
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
    replaceWidget(m_statusLayout, &m_statusView, view);
}

void DashboardPage::setWarnings(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("predictions")).toArray();
    m_warningTable->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject item = items.at(row).toObject();
        const QStringList values{
            item.value(QStringLiteral("stationName")).toString(),
            item.value(QStringLiteral("predictionTime")).toString(),
            item.value(QStringLiteral("horizon")).toString(),
            QString::number(item.value(QStringLiteral("predictedLoad")).toDouble() * 100.0,
                            'f', 0) + QStringLiteral("%"),
            QString::number(item.value(QStringLiteral("predictedAvailableCount")).toInt()),
            statusText(item.value(QStringLiteral("peakLevel")).toString())
        };
        for (int column = 0; column < values.size(); ++column) {
            m_warningTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}

PilePage::PilePage(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 24); root->setSpacing(14);
    root->addWidget(label(QStringLiteral("充电桩管理"), "pageTitle"));
    root->addWidget(label(QStringLiteral("查看设备状态与累计使用情况；危险操作需要二次确认"), "subtitle"));
    auto *tools = new QHBoxLayout; m_filterHint = label(QStringLiteral("当前：全部站点"), "caption"); tools->addWidget(m_filterHint); tools->addStretch();
    m_statusFilter = new QComboBox(this); m_statusFilter->addItems({QStringLiteral("全部状态"), QStringLiteral("空闲"), QStringLiteral("已预约"), QStringLiteral("充电中"), QStringLiteral("故障"), QStringLiteral("离线"), QStringLiteral("重启中")}); tools->addWidget(m_statusFilter);
    auto *refresh = button(QStringLiteral("刷新电桩"), this);
    tools->addWidget(refresh); root->addLayout(tools); prepareTable(m_table);
    root->addWidget(m_table, 1);
    connect(refresh, &QPushButton::clicked, this, &PilePage::refreshRequested);
    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { applyFilter(); });
}

void PilePage::setPiles(const QJsonObject &data)
{
    m_piles = data.value(QStringLiteral("piles")).toArray(); applyFilter();
}

void PilePage::applyFilter()
{
    static const QStringList statuses{{}, QStringLiteral("AVAILABLE"), QStringLiteral("RESERVED"),
        QStringLiteral("CHARGING"), QStringLiteral("FAULT"), QStringLiteral("OFFLINE"),
        QStringLiteral("RESTARTING")};
    const QString wanted = statuses.value(m_statusFilter ? m_statusFilter->currentIndex() : 0);
    QJsonArray piles;
    for (const QJsonValue &value : m_piles) {
        if (wanted.isEmpty() || value.toObject().value(QStringLiteral("status")).toString() == wanted)
            piles.append(value);
    }
    m_table->clear(); m_table->setRowCount(piles.size()); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("站点"), QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"), QStringLiteral("次数"), QStringLiteral("分钟"), QStringLiteral("操作")});
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QStringList values = {pile.value(QStringLiteral("pileNo")).toString(), pile.value(QStringLiteral("stationName")).toString(), pile.value(QStringLiteral("type")).toString() == QStringLiteral("FAST") ? QStringLiteral("快充") : QStringLiteral("慢充"), QString::number(pile.value(QStringLiteral("powerKw")).toDouble()) + QStringLiteral(" kW"), statusText(pile.value(QStringLiteral("status")).toString()), QString::number(pile.value(QStringLiteral("totalChargeCount")).toInt()), QString::number(pile.value(QStringLiteral("totalChargeMinutes")).toInt()) + QStringLiteral(" 分钟")};
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
    m_actionBusy = busy; m_table->setEnabled(!busy);
}

void PilePage::setStationFilterLabel(const QString &stationName)
{
    m_filterHint->setText(stationName.isEmpty() ? QStringLiteral("当前：全部站点")
                                                : QStringLiteral("当前：%1").arg(stationName));
}

StationPage::StationPage(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 24); root->setSpacing(14);
    auto *heading = new QHBoxLayout; auto *titles = new QVBoxLayout; titles->addWidget(label(QStringLiteral("充电站管理"), "pageTitle")); titles->addWidget(label(QStringLiteral("管理站点基础信息并查看站内电桩"), "subtitle")); heading->addLayout(titles); heading->addStretch();
    auto *refresh = button(QStringLiteral("刷新站点"), this);
    m_create = button(QStringLiteral("新增站点"), this);
    refresh->setProperty("kind", "secondary"); heading->addWidget(refresh); heading->addWidget(m_create); root->addLayout(heading);
    auto *split = new QHBoxLayout; prepareTable(m_table); split->addWidget(m_table, 3);
    auto *detailPanel = panel(); auto *detailLayout = new QVBoxLayout(detailPanel); detailLayout->addWidget(label(QStringLiteral("站内电桩"), "pageTitle")); m_pileDetailTitle = label(QStringLiteral("选择左侧站点查看详情"), "caption"); detailLayout->addWidget(m_pileDetailTitle);
    m_pileDetail = new QTableWidget(0, 4, detailPanel); m_pileDetail->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态")}); prepareTable(m_pileDetail); detailLayout->addWidget(m_pileDetail); split->addWidget(detailPanel, 2); root->addLayout(split, 1);
    connect(refresh, &QPushButton::clicked, this, &StationPage::refreshRequested);
    connect(m_create, &QPushButton::clicked, this, &StationPage::openCreateDialog);
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
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 24); root->setSpacing(14);
    root->addWidget(label(QStringLiteral("用户管理"), "pageTitle")); root->addWidget(label(QStringLiteral("按手机号查询用户并维护账户状态"), "subtitle"));
    auto *tools = new QHBoxLayout; auto *refresh = button(QStringLiteral("刷新用户"), this); refresh->setProperty("kind", "secondary");
    auto *search = button(QStringLiteral("查询"), this); auto *clear = button(QStringLiteral("清空"), this); clear->setProperty("kind", "secondary");
    m_search->setPlaceholderText(QStringLiteral("输入手机号关键词")); tools->addWidget(m_search, 1); tools->addWidget(search); tools->addWidget(clear); tools->addWidget(refresh); root->addLayout(tools); prepareTable(m_table); root->addWidget(m_table, 1);
    connect(refresh, &QPushButton::clicked, this, &UserPage::refreshRequested);
    connect(m_search, &QLineEdit::returnPressed, this, [this]() { emit searchRequested(phoneKeyword()); });
    connect(search, &QPushButton::clicked, this, [this]() { emit searchRequested(phoneKeyword()); });
    connect(clear, &QPushButton::clicked, this, [this]() { m_search->clear(); emit searchRequested(QString()); });
}

void StationPage::setPileDetails(const QJsonArray &piles)
{
    m_pileDetailTitle->setText(piles.isEmpty() ? QStringLiteral("该站暂无电桩")
                                               : QStringLiteral("共 %1 个电桩").arg(piles.size()));
    m_pileDetail->setRowCount(piles.size());
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QString type = pile.value(QStringLiteral("type")).toString();
        const QStringList values{
            pile.value(QStringLiteral("pileNo")).toString(),
            type == QStringLiteral("FAST") ? QStringLiteral("快充")
                : type == QStringLiteral("SLOW") ? QStringLiteral("慢充") : type,
            QString::number(pile.value(QStringLiteral("powerKw")).toDouble()) + QStringLiteral(" kW"),
            statusText(pile.value(QStringLiteral("status")).toString())
        };
        for (int column = 0; column < values.size(); ++column) {
            m_pileDetail->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}

void StationPage::setPileDetailStatus(const QString &message)
{
    m_pileDetailTitle->setText(message);
}

void StationPage::setCreateBusy(bool busy) { m_create->setEnabled(!busy); }

QString UserPage::phoneKeyword() const { return m_search->text().trimmed(); }

void UserPage::setUsers(const QJsonObject &data)
{
    const QJsonArray users = data.value(QStringLiteral("users")).toArray();
    m_table->clear(); m_table->setRowCount(users.size()); m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"), QStringLiteral("余额"), QStringLiteral("注册时间"), QStringLiteral("状态"), QStringLiteral("操作")});
    for (int row = 0; row < users.size(); ++row) {
        const QJsonObject user = users.at(row).toObject();
        const QStringList values = {QString::number(user.value(QStringLiteral("userId")).toInteger()), user.value(QStringLiteral("phone")).toString(), user.value(QStringLiteral("nickname")).toString(), QStringLiteral("¥") + QString::number(user.value(QStringLiteral("balanceFen")).toInteger() / 100.0, 'f', 2), user.value(QStringLiteral("createdAt")).toString(), statusText(user.value(QStringLiteral("status")).toString())};
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
    m_actionBusy = busy; m_table->setEnabled(!busy);
}
