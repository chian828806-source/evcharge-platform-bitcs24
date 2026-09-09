#include "adminpages.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFont>
#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLegend>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

namespace {
QString statusText(const QString &status);

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
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setMinimumSectionSize(60);
    table->verticalHeader()->setDefaultSectionSize(60);
    table->horizontalHeader()->setMinimumSectionSize(40);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

QTableWidgetItem *tableItem(const QString &text, bool centered = false)
{
    auto *item = new QTableWidgetItem(text);
    item->setToolTip(text);
    item->setTextAlignment(centered ? Qt::AlignCenter
                                    : Qt::AlignLeft | Qt::AlignVCenter);
    return item;
}

QTableWidgetItem *statusItem(const QString &status)
{
    auto *item = tableItem(statusText(status), true);
    const bool positive = status == QStringLiteral("AVAILABLE")
        || status == QStringLiteral("NORMAL") || status == QStringLiteral("LOW")
        || status == QStringLiteral("COMPLETED");
    const bool warning = status == QStringLiteral("RESERVED")
        || status == QStringLiteral("CHARGING") || status == QStringLiteral("RESTARTING")
        || status == QStringLiteral("MEDIUM") || status == QStringLiteral("CREATED")
        || status == QStringLiteral("PENDING_PAYMENT");
    item->setForeground(QColor(positive ? QStringLiteral("#087f69")
        : warning ? QStringLiteral("#a35a00") : QStringLiteral("#b53b34")));
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    return item;
}

void styleChart(QChart *chart)
{
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(false);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setAlignment(Qt::AlignBottom);
    QFont titleFont;
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    chart->setTitleFont(titleFont);
    chart->setTitleBrush(QColor(QStringLiteral("#172b28")));
    chart->legend()->setLabelColor(QColor(QStringLiteral("#536763")));
}

QToolButton *tableActionButton(QTableWidget *table, int row, int column,
                               const QString &text)
{
    auto *cell = new QWidget(table);
    cell->setObjectName(QStringLiteral("tableActionCell"));
    cell->setAttribute(Qt::WA_TranslucentBackground);
    cell->setMinimumHeight(68);
    auto *layout = new QHBoxLayout(cell);
    layout->setContentsMargins(8, 14, 8, 14);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);
    auto *action = new QToolButton(cell);
    action->setText(text);
    action->setFixedSize(104, 40);
    action->setStyleSheet(QStringLiteral(
        "QToolButton { background:#087f69; color:white; border:1px solid #087f69; "
        "border-radius:11px; padding:0px 12px; margin:0px; font-size:15px; font-weight:700; }"
        "QToolButton:hover { background:#066b59; border-color:#066b59; }"
        "QToolButton:disabled { background:#c7d5d1; border-color:#c7d5d1; color:#758681; }"));
    layout->addWidget(action);
    table->setCellWidget(row, column, cell);
    table->verticalHeader()->setSectionResizeMode(row, QHeaderView::Fixed);
    table->verticalHeader()->resizeSection(row, 72);
    return action;
}

void configureActionColumn(QTableWidget *table, int actionColumn)
{
    auto *header = table->horizontalHeader();
    for (int column = 0; column < table->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Stretch);
    header->setSectionResizeMode(actionColumn, QHeaderView::Fixed);
    table->setColumnWidth(actionColumn, 136);
    table->horizontalScrollBar()->setValue(0);
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
        {QStringLiteral("CREATED"), QStringLiteral("已创建")},
        {QStringLiteral("PENDING_PAYMENT"), QStringLiteral("待支付")},
        {QStringLiteral("COMPLETED"), QStringLiteral("已完成")},
        {QStringLiteral("CANCELLED"), QStringLiteral("已取消")},
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

void showEmptyState(QTableWidget *table, int columns, const QString &message)
{
    table->clearContents();
    table->clearSpans();
    table->setRowCount(1);
    auto *item = new QTableWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    table->setItem(0, 0, item);
    if (columns > 1) table->setSpan(0, 0, 1, columns);
}
}

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 24); root->setSpacing(16);
    auto *hero = panel();
    hero->setProperty("variant", "dashboardHero");
    auto *top = new QHBoxLayout(hero);
    top->setContentsMargins(22, 16, 18, 16);
    auto *heading = new QVBoxLayout;
    heading->setSpacing(5);
    heading->addWidget(label(QStringLiteral("欢迎回来，运营管理员"), "pageTitle"));
    heading->addWidget(label(QStringLiteral("实时掌握充电网络营收、设备运行与负荷趋势"), "subtitle"));
    top->addLayout(heading); top->addStretch();
    auto *heroMark = label(QStringLiteral("⚡  EVCHARGE 运营中心"), "heroMark");
    top->addWidget(heroMark);
    auto *refresh = button(QStringLiteral("刷新 Dashboard"), this);
    auto *seven = button(QStringLiteral("近 7 日"), this);
    auto *thirty = button(QStringLiteral("近 30 日"), this);
    seven->setProperty("kind", "secondary"); thirty->setProperty("kind", "secondary");
    top->addWidget(refresh);
    root->addWidget(hero);
    auto *metrics = new QHBoxLayout;
    const QStringList names{QStringLiteral("今日营收"), QStringLiteral("本月营收"), QStringLiteral("累计营收")};
    QLabel **values[]{&m_today, &m_month, &m_total};
    for (int i = 0; i < names.size(); ++i) { auto *card = panel(); card->setProperty("variant", "metric"); auto *box = new QVBoxLayout(card);
        box->setContentsMargins(20, 16, 20, 16); auto *metricTop = new QHBoxLayout;
        metricTop->addWidget(label(names.at(i), "metricTitle")); metricTop->addStretch();
        metricTop->addWidget(label(i == 0 ? QStringLiteral("今日") : i == 1 ? QStringLiteral("本月") : QStringLiteral("累计"), "metricTag"));
        box->addLayout(metricTop);
        *values[i] = label(QStringLiteral("¥ --"), "metricValue"); box->addWidget(*values[i]); metrics->addWidget(card, 1); }
    root->addLayout(metrics);
    auto *period = new QHBoxLayout; period->addWidget(label(QStringLiteral("营收趋势"), "sectionTitle")); period->addStretch(); period->addWidget(seven); period->addWidget(thirty); root->addLayout(period);
    m_chartLayout = new QVBoxLayout;
    auto *chartPanel = panel(); chartPanel->setLayout(m_chartLayout); m_trendView = label(QStringLiteral("等待趋势数据"), "caption"); m_chartLayout->addWidget(m_trendView); root->addWidget(chartPanel, 2);
    auto *bottom = new QHBoxLayout;
    auto *statusPanel = panel(); m_statusLayout = new QVBoxLayout(statusPanel); m_statusLayout->addWidget(label(QStringLiteral("电桩状态"), "sectionTitle"));
    m_statusView = label(QStringLiteral("等待状态数据"), "caption"); m_statusLayout->addWidget(m_statusView); bottom->addWidget(statusPanel, 1);
    auto *warningPanel = panel(); auto *warningLayout = new QVBoxLayout(warningPanel); auto *warningTop = new QHBoxLayout;
    warningTop->addWidget(label(QStringLiteral("负荷预警"), "sectionTitle")); warningTop->addStretch();
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
    m_today->setText(QStringLiteral("¥ %1").arg(yuan(data.value(QStringLiteral("todayRevenueFen")).toInteger())));
    m_month->setText(QStringLiteral("¥ %1").arg(yuan(data.value(QStringLiteral("monthRevenueFen")).toInteger())));
    m_total->setText(QStringLiteral("¥ %1").arg(yuan(data.value(QStringLiteral("totalRevenueFen")).toInteger())));
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
    if (points.isEmpty()) {
        replaceWidget(m_chartLayout, &m_trendView,
                      label(QStringLiteral("暂无趋势数据"), "caption"));
        return;
    }
    auto *axisX = new QCategoryAxis;
    axisX->setLabelsAngle(-45);
    qreal primaryMaximum = 0.0;
    qreal orderMaximum = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        const QJsonObject point = points.at(i).toObject();
        const qreal revenue = point.value(QStringLiteral("revenueFen")).toDouble() / 100.0;
        const qreal energy = point.value(QStringLiteral("energyKwh")).toDouble();
        const qreal orders = point.value(QStringLiteral("orderCount")).toInt();
        revenueSeries->append(i, revenue);
        energySeries->append(i, energy);
        orderSeries->append(i, orders);
        primaryMaximum = qMax(primaryMaximum, qMax(revenue, energy));
        orderMaximum = qMax(orderMaximum, orders);
        axisX->append(point.value(QStringLiteral("date")).toString(), i + 0.5);
    }
    auto *chart = new QChart;
    revenueSeries->setPen(QPen(QColor(QStringLiteral("#087f69")), 3));
    energySeries->setPen(QPen(QColor(QStringLiteral("#2f80c9")), 3));
    orderSeries->setPen(QPen(QColor(QStringLiteral("#d97706")), 3));
    revenueSeries->setPointsVisible(true);
    energySeries->setPointsVisible(true);
    orderSeries->setPointsVisible(true);
    revenueSeries->setMarkerSize(7.0);
    energySeries->setMarkerSize(7.0);
    orderSeries->setMarkerSize(7.0);
    chart->addSeries(revenueSeries); chart->addSeries(energySeries); chart->addSeries(orderSeries);
    chart->addAxis(axisX, Qt::AlignBottom);
    auto *axisY = new QValueAxis;
    axisY->setRange(primaryMaximum > 0.0 ? 0.0 : -1.0,
                    primaryMaximum > 0.0 ? primaryMaximum * 1.15 : 1.0);
    axisY->setLabelFormat(QStringLiteral("%.1f"));
    axisY->setTickCount(5);
    chart->addAxis(axisY, Qt::AlignLeft);
    for (QLineSeries *series : {revenueSeries, energySeries}) {
        series->attachAxis(axisX); series->attachAxis(axisY);
    }
    auto *orderAxis = new QValueAxis;
    orderAxis->setRange(orderMaximum > 0.0 ? 0.0 : -1.0,
                        orderMaximum > 0.0 ? orderMaximum * 1.15 : 1.0);
    orderAxis->setLabelFormat(QStringLiteral("%.0f"));
    orderAxis->setTickCount(5);
    orderAxis->setTitleText(QStringLiteral("订单数"));
    chart->addAxis(orderAxis, Qt::AlignRight);
    orderSeries->attachAxis(axisX); orderSeries->attachAxis(orderAxis);
    chart->setTitle(QStringLiteral("近 %1 日营收趋势").arg(data.value(QStringLiteral("days")).toInt()));
    styleChart(chart);
    auto *view = new QChartView(chart, this);
    view->setMinimumHeight(270);
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
    const QList<QColor> colors{QColor(QStringLiteral("#10a37f")), QColor(QStringLiteral("#2f80c9")),
                               QColor(QStringLiteral("#d97706")), QColor(QStringLiteral("#c9473d")),
                               QColor(QStringLiteral("#7a8b86"))};
    int sliceIndex = 0;
    for (QPieSlice *slice : series->slices()) {
        slice->setColor(colors.at(sliceIndex++ % colors.size()));
        slice->setLabelVisible(true);
        slice->setLabelColor(QColor(QStringLiteral("#35514b")));
        slice->setLabel(QStringLiteral("%1  %2%").arg(statusText(slice->label()))
                        .arg(slice->percentage() * 100.0, 0, 'f', 0));
    }
    styleChart(chart);
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    replaceWidget(m_statusLayout, &m_statusView, view);
}

void DashboardPage::setWarnings(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("predictions")).toArray();
    m_warningTable->clearContents();
    m_warningTable->clearSpans();
    if (items.isEmpty()) {
        showEmptyState(m_warningTable, 6, QStringLiteral("暂无负荷预警"));
        return;
    }
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
            m_warningTable->setItem(row, column,
                column == 5 ? statusItem(item.value(QStringLiteral("peakLevel")).toString())
                            : tableItem(values.at(column), column >= 2));
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
    m_table->clear(); m_table->clearSpans(); m_table->setRowCount(piles.size()); m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩号"), QStringLiteral("站点"), QStringLiteral("类型"), QStringLiteral("额定功率"), QStringLiteral("状态"), QStringLiteral("设备"), QStringLiteral("实时功率"), QStringLiteral("温度/故障"), QStringLiteral("次数"), QStringLiteral("分钟"), QStringLiteral("操作")});
    if (piles.isEmpty()) {
        showEmptyState(m_table, 11, wanted.isEmpty() ? QStringLiteral("暂无电桩")
                                                   : QStringLiteral("当前筛选条件下暂无电桩"));
        return;
    }
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QString fault = pile.value(QStringLiteral("faultCode")).toString();
        const QStringList values = {pile.value(QStringLiteral("pileNo")).toString(), pile.value(QStringLiteral("stationName")).toString(), pile.value(QStringLiteral("type")).toString() == QStringLiteral("FAST") ? QStringLiteral("快充") : QStringLiteral("慢充"), QString::number(pile.value(QStringLiteral("powerKw")).toDouble()) + QStringLiteral(" kW"), statusText(pile.value(QStringLiteral("status")).toString()), pile.value(QStringLiteral("deviceOnline")).toBool() ? QStringLiteral("在线") : QStringLiteral("未接入/离线"), QString::number(pile.value(QStringLiteral("measuredPowerKw")).toDouble(), 'f', 1) + QStringLiteral(" kW"), fault.isEmpty() ? QString::number(pile.value(QStringLiteral("temperatureC")).toDouble(), 'f', 1) + QStringLiteral(" °C") : fault + QStringLiteral(" ") + pile.value(QStringLiteral("faultMessage")).toString(), QString::number(pile.value(QStringLiteral("totalChargeCount")).toInt()), QString::number(pile.value(QStringLiteral("totalChargeMinutes")).toInt()) + QStringLiteral(" 分钟")};
        for (int column = 0; column < values.size(); ++column) {
            m_table->setItem(row, column,
                column == 4 ? statusItem(pile.value(QStringLiteral("status")).toString())
                            : tableItem(values.at(column), column != 1));
        }
        auto *restart = tableActionButton(m_table, row, 10, QStringLiteral("远程重启"));
        const QString status = pile.value(QStringLiteral("status")).toString();
        restart->setEnabled(!m_actionBusy && status != QStringLiteral("RESERVED") && status != QStringLiteral("CHARGING") && status != QStringLiteral("RESTARTING"));
        const qint64 pileId = pile.value(QStringLiteral("pileId")).toInteger();
        connect(restart, &QToolButton::clicked, this, [this, pileId]() { emit restartRequested(pileId); });
    }
    configureActionColumn(m_table, 10);
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
    m_table->clear(); m_table->clearSpans(); m_table->setRowCount(stations.size()); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("名称"), QStringLiteral("地址"), QStringLiteral("经度"), QStringLiteral("纬度"), QStringLiteral("电桩"), QStringLiteral("在线率"), QStringLiteral("操作")});
    if (stations.isEmpty()) {
        showEmptyState(m_table, 8, QStringLiteral("暂无站点"));
        return;
    }
    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations.at(row).toObject();
        const QStringList values = {station.value(QStringLiteral("stationNo")).toString(), station.value(QStringLiteral("name")).toString(), station.value(QStringLiteral("address")).toString(), QString::number(station.value(QStringLiteral("longitude")).toDouble(), 'f', 6), QString::number(station.value(QStringLiteral("latitude")).toDouble(), 'f', 6), QString::number(station.value(QStringLiteral("pileCount")).toInt()), QString::number(station.value(QStringLiteral("onlineRate")).toDouble() * 100, 'f', 1) + '%'};
        for (int column = 0; column < values.size(); ++column)
            m_table->setItem(row, column, tableItem(values.at(column), column != 1 && column != 2));
        auto *view = tableActionButton(m_table, row, 7, QStringLiteral("查看电桩"));
        const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
        connect(view, &QToolButton::clicked, this, [this, stationId]() { emit stationPilesRequested(stationId); });
    }
    configureActionColumn(m_table, 7);
}

void StationPage::openCreateDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新增充电站"));
    dialog.setMinimumWidth(460);
    auto *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(28, 24, 28, 24);
    dialogLayout->setSpacing(16);
    dialogLayout->addWidget(label(QStringLiteral("新增充电站"), "dialogTitle"));
    dialogLayout->addWidget(label(QStringLiteral("填写站点基础信息，创建后可继续查看和维护站内电桩。"),
                                  "dialogDescription"));
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    QLineEdit name, address;
    QDoubleSpinBox longitude, latitude, price; QSpinBox count;
    longitude.setRange(-180, 180); latitude.setRange(-90, 90); count.setRange(1, 100); count.setValue(4);
    price.setRange(0.01, 100.0); price.setDecimals(2); price.setValue(1.20); price.setSuffix(QStringLiteral(" 元/度"));
    name.setPlaceholderText(QStringLiteral("请输入充电站名称"));
    address.setPlaceholderText(QStringLiteral("请输入详细地址"));
    form->addRow(QStringLiteral("站名"), &name); form->addRow(QStringLiteral("地址"), &address); form->addRow(QStringLiteral("经度"), &longitude); form->addRow(QStringLiteral("纬度"), &latitude); form->addRow(QStringLiteral("电桩数"), &count); form->addRow(QStringLiteral("充电单价"), &price);
    dialogLayout->addLayout(form);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建站点"));
    buttons.button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons.button(QDialogButtonBox::Cancel)->setProperty("kind", "secondary");
    dialogLayout->addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (name.text().trimmed().isEmpty() || address.text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"),
                             QStringLiteral("请填写站名和地址。"));
        return;
    }
    emit createRequested({{QStringLiteral("name"), name.text().trimmed()}, {QStringLiteral("address"), address.text().trimmed()}, {QStringLiteral("longitude"), longitude.value()}, {QStringLiteral("latitude"), latitude.value()}, {QStringLiteral("pileCount"), count.value()}, {QStringLiteral("priceFenPerKwh"), qRound64(price.value() * 100.0)}});
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
            m_pileDetail->setItem(row, column,
                column == 3 ? statusItem(pile.value(QStringLiteral("status")).toString())
                            : tableItem(values.at(column), true));
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
    m_table->clear(); m_table->clearSpans(); m_table->setRowCount(users.size()); m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"), QStringLiteral("余额"), QStringLiteral("注册时间"), QStringLiteral("状态"), QStringLiteral("操作")});
    if (users.isEmpty()) {
        showEmptyState(m_table, 7, m_search->text().trimmed().isEmpty()
            ? QStringLiteral("暂无用户") : QStringLiteral("未找到匹配用户"));
        return;
    }
    for (int row = 0; row < users.size(); ++row) {
        const QJsonObject user = users.at(row).toObject();
        const QStringList values = {QString::number(user.value(QStringLiteral("userId")).toInteger()), user.value(QStringLiteral("phone")).toString(), user.value(QStringLiteral("nickname")).toString(), QStringLiteral("¥") + QString::number(user.value(QStringLiteral("balanceFen")).toInteger() / 100.0, 'f', 2), user.value(QStringLiteral("createdAt")).toString(), statusText(user.value(QStringLiteral("status")).toString())};
        for (int column = 0; column < values.size(); ++column) {
            m_table->setItem(row, column,
                column == 5 ? statusItem(user.value(QStringLiteral("status")).toString())
                            : tableItem(values.at(column), column == 0 || column == 3));
        }
        const bool frozen = user.value(QStringLiteral("status")).toString() == QStringLiteral("FROZEN");
        auto *change = tableActionButton(m_table, row, 6,
            frozen ? QStringLiteral("解冻") : QStringLiteral("冻结"));
        change->setEnabled(!m_actionBusy);
        const qint64 userId = user.value(QStringLiteral("userId")).toInteger();
        connect(change, &QToolButton::clicked, this, [this, userId, frozen]() { emit statusChangeRequested(userId, !frozen); });
    }
    configureActionColumn(m_table, 6);
}

void UserPage::setActionBusy(bool busy)
{
    m_actionBusy = busy; m_table->setEnabled(!busy);
}

OrderPage::OrderPage(QWidget *parent)
    : QWidget(parent), m_search(new QLineEdit(this)),
      m_statusFilter(new QComboBox(this)), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 24);
    root->setSpacing(14);

    auto *heading = new QHBoxLayout;
    auto *headingText = new QVBoxLayout;
    headingText->setSpacing(5);
    headingText->addWidget(label(QStringLiteral("订单管理"), "pageTitle"));
    headingText->addWidget(label(QStringLiteral("查询全平台充电订单，快速核对用户、站点与结算信息"), "subtitle"));
    m_resultSummary = label(QStringLiteral("等待查询"), "summaryBadge");
    heading->addLayout(headingText);
    heading->addStretch();
    heading->addWidget(m_resultSummary, 0, Qt::AlignTop);
    root->addLayout(heading);

    auto *filterPanel = panel();
    filterPanel->setProperty("variant", "orderFilter");
    auto *filterLayout = new QVBoxLayout(filterPanel);
    filterLayout->setContentsMargins(18, 14, 18, 16);
    filterLayout->setSpacing(10);
    filterLayout->addWidget(label(QStringLiteral("筛选条件"), "filterTitle"));
    auto *tools = new QHBoxLayout;
    tools->setSpacing(10);
    m_search->setPlaceholderText(QStringLiteral("输入用户手机号关键词"));
    m_search->setClearButtonEnabled(true);
    m_statusFilter->addItem(QStringLiteral("全部状态"), QString());
    m_statusFilter->addItem(QStringLiteral("已创建"), QStringLiteral("CREATED"));
    m_statusFilter->addItem(QStringLiteral("充电中"), QStringLiteral("CHARGING"));
    m_statusFilter->addItem(QStringLiteral("待支付"), QStringLiteral("PENDING_PAYMENT"));
    m_statusFilter->addItem(QStringLiteral("已完成"), QStringLiteral("COMPLETED"));
    m_statusFilter->addItem(QStringLiteral("已取消"), QStringLiteral("CANCELLED"));
    auto *query = button(QStringLiteral("查询订单"), this);
    auto *clear = button(QStringLiteral("重置条件"), this);
    clear->setProperty("kind", "secondary");
    tools->addWidget(label(QStringLiteral("用户手机号"), "fieldLabel"));
    tools->addWidget(m_search, 1);
    tools->addWidget(label(QStringLiteral("订单状态"), "fieldLabel"));
    tools->addWidget(m_statusFilter);
    tools->addWidget(query);
    tools->addWidget(clear);
    filterLayout->addLayout(tools);
    root->addWidget(filterPanel);

    auto *listHeading = new QHBoxLayout;
    listHeading->addWidget(label(QStringLiteral("订单列表"), "sectionTitle"));
    listHeading->addStretch();
    m_loadState = label(QString(), "loadState");
    m_loadState->setVisible(false);
    listHeading->addWidget(m_loadState);
    root->addLayout(listHeading);
    prepareTable(m_table);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->verticalHeader()->setMinimumSectionSize(54);
    m_table->verticalHeader()->setDefaultSectionSize(54);
    root->addWidget(m_table, 1);

    auto *pagination = new QFrame(this);
    pagination->setObjectName(QStringLiteral("orderPagination"));
    auto *pages = new QHBoxLayout(pagination);
    pages->setContentsMargins(12, 8, 12, 8);
    pages->addStretch();
    m_previous = button(QStringLiteral("上一页"), this);
    m_next = button(QStringLiteral("下一页"), this);
    m_previous->setProperty("kind", "secondary"); m_next->setProperty("kind", "secondary");
    m_pageLabel = label(QStringLiteral("第 1 页"), "caption");
    pages->addWidget(m_previous); pages->addWidget(m_pageLabel); pages->addWidget(m_next);
    pages->addStretch();
    root->addWidget(pagination);
    connect(query, &QPushButton::clicked, this, [this]() { requestPage(1); });
    connect(m_search, &QLineEdit::returnPressed, this, [this]() { requestPage(1); });
    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { requestPage(1); });
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_search->clear();
        if (m_statusFilter->currentIndex() == 0) requestPage(1);
        else m_statusFilter->setCurrentIndex(0);
    });
    connect(m_previous, &QPushButton::clicked, this, [this]() { requestPage(m_page - 1); });
    connect(m_next, &QPushButton::clicked, this, [this]() { requestPage(m_page + 1); });
}

void OrderPage::requestPage(int page)
{
    emit queryRequested(qMax(1, page), m_search->text().trimmed(),
                        m_statusFilter->currentData().toString());
}

void OrderPage::setLoading()
{
    m_pageLabel->setText(QStringLiteral("正在加载…"));
    m_resultSummary->setText(QStringLiteral("查询中"));
    m_loadState->setText(QStringLiteral("● 正在加载订单"));
    m_loadState->setProperty("error", false);
    m_loadState->style()->unpolish(m_loadState);
    m_loadState->style()->polish(m_loadState);
    m_loadState->setVisible(true);
    m_previous->setEnabled(false); m_next->setEnabled(false);
}

void OrderPage::setLoadError(const QString &message)
{
    m_pageLabel->setText(message);
    m_resultSummary->setText(QStringLiteral("保留上次结果"));
    m_loadState->setText(QStringLiteral("加载失败 · 请重试"));
    m_loadState->setProperty("error", true);
    m_loadState->style()->unpolish(m_loadState);
    m_loadState->style()->polish(m_loadState);
    m_loadState->setVisible(true);
    m_previous->setEnabled(m_page > 1);
    const int pageCount = qMax(1, (m_total + m_pageSize - 1) / m_pageSize);
    m_next->setEnabled(m_page < pageCount);
}

void OrderPage::setOrders(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("items")).toArray();
    m_page = data.value(QStringLiteral("page")).toInt(1);
    m_pageSize = data.value(QStringLiteral("pageSize")).toInt(20);
    m_total = data.value(QStringLiteral("total")).toInt();
    m_loadState->setVisible(false);
    m_resultSummary->setText(QStringLiteral("共 %1 条订单").arg(m_total));
    const int pageCount = qMax(1, (m_total + m_pageSize - 1) / m_pageSize);
    m_pageLabel->setText(QStringLiteral("第 %1 / %2 页，共 %3 条")
                         .arg(m_page).arg(pageCount).arg(m_total));
    m_previous->setEnabled(m_page > 1);
    m_next->setEnabled(m_page < pageCount);
    m_table->clear(); m_table->clearSpans();
    m_table->setColumnCount(9); m_table->setRowCount(items.size());
    m_table->setHorizontalHeaderLabels({QStringLiteral("订单号"), QStringLiteral("用户"),
        QStringLiteral("站点"), QStringLiteral("电桩"), QStringLiteral("状态"),
        QStringLiteral("电量"), QStringLiteral("时长"), QStringLiteral("金额"),
        QStringLiteral("创建时间")});
    if (items.isEmpty()) {
        showEmptyState(m_table, 9, QStringLiteral("当前条件下暂无订单"));
        return;
    }
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject order = items.at(row).toObject();
        const QString nickname = order.value(QStringLiteral("userNickname")).toString();
        const QString phone = order.value(QStringLiteral("userPhone")).toString();
        const QString user = nickname.isEmpty()
            ? (phone.isEmpty() ? QStringLiteral("—") : phone)
            : (phone.isEmpty() ? nickname : QStringLiteral("%1（%2）").arg(nickname, phone));
        const auto textOrDash = [&order](const QString &key) {
            const QString value = order.value(key).toString();
            return value.isEmpty() ? QStringLiteral("—") : value;
        };
        const QJsonValue energy = order.value(QStringLiteral("energyKwh"));
        const QJsonValue minutes = order.value(QStringLiteral("chargeMinutes"));
        const QJsonValue amount = order.value(QStringLiteral("amountFen"));
        const QStringList values{
            textOrDash(QStringLiteral("orderNo")), user,
            textOrDash(QStringLiteral("stationName")),
            textOrDash(QStringLiteral("pileNo")),
            statusText(order.value(QStringLiteral("status")).toString()),
            energy.isDouble() ? QString::number(energy.toDouble(), 'f', 2) + QStringLiteral(" kWh")
                              : QStringLiteral("—"),
            minutes.isDouble() ? QString::number(minutes.toInt()) + QStringLiteral(" 分钟")
                               : QStringLiteral("—"),
            amount.isDouble() ? QStringLiteral("¥") + QString::number(amount.toInteger() / 100.0, 'f', 2)
                              : QStringLiteral("—"),
            textOrDash(QStringLiteral("createdAt"))
        };
        for (int column = 0; column < values.size(); ++column) {
            m_table->setItem(row, column, column == 4
                ? statusItem(order.value(QStringLiteral("status")).toString())
                : tableItem(values.at(column), column != 1 && column != 2));
        }
    }
    auto *header = m_table->horizontalHeader();
    for (int column = 0; column < m_table->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Fixed);
    const QList<int> widths{200, 150, 150, 115, 90, 100, 90, 100, 170};
    for (int column = 0; column < widths.size(); ++column)
        m_table->setColumnWidth(column, widths.at(column));
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(8, QHeaderView::Stretch);
    header->setStretchLastSection(false);
}
