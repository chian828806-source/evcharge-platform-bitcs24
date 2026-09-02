#include "adminpages.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
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
    m_today = new QLabel(this);
    m_month = new QLabel(this);
    m_total = new QLabel(this);
    root->addWidget(refresh);
    root->addWidget(seven);
    root->addWidget(thirty);
    root->addWidget(m_today);
    root->addWidget(m_month);
    root->addWidget(m_total);
    m_chartLayout = new QVBoxLayout;
    root->addLayout(m_chartLayout, 1);
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
    auto *series = new QLineSeries;
    const QJsonArray points = data.value(QStringLiteral("points")).toArray();
    for (int i = 0; i < points.size(); ++i) {
        series->append(i, points.at(i).toObject().value(QStringLiteral("revenueFen")).toDouble() / 100.0);
    }
    auto *chart = new QChart;
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->legend()->hide();
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
    replaceWidget(m_chartLayout, &m_statusView, view);
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
        restart->setEnabled(status != QStringLiteral("RESERVED") && status != QStringLiteral("CHARGING") && status != QStringLiteral("RESTARTING"));
        const qint64 pileId = pile.value(QStringLiteral("pileId")).toInteger();
        connect(restart, &QPushButton::clicked, this, [this, pileId]() { emit restartRequested(pileId); });
        m_table->setCellWidget(row, 7, restart);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

StationPage::StationPage(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    auto *root = new QVBoxLayout(this);
    auto *refresh = button(QStringLiteral("刷新站点"), this);
    auto *create = button(QStringLiteral("新增站点"), this);
    root->addWidget(refresh); root->addWidget(create); root->addWidget(m_table, 1);
    connect(refresh, &QPushButton::clicked, this, &StationPage::refreshRequested);
    connect(create, &QPushButton::clicked, this, &StationPage::openCreateDialog);
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
    QDoubleSpinBox longitude, latitude; QSpinBox count;
    longitude.setRange(-180, 180); latitude.setRange(-90, 90); count.setRange(1, 100); count.setValue(4);
    form.addRow(QStringLiteral("站名"), &name); form.addRow(QStringLiteral("地址"), &address); form.addRow(QStringLiteral("经度"), &longitude); form.addRow(QStringLiteral("纬度"), &latitude); form.addRow(QStringLiteral("电桩数"), &count);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) emit createRequested({{QStringLiteral("name"), name.text().trimmed()}, {QStringLiteral("address"), address.text().trimmed()}, {QStringLiteral("longitude"), longitude.value()}, {QStringLiteral("latitude"), latitude.value()}, {QStringLiteral("pileCount"), count.value()}});
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
        const qint64 userId = user.value(QStringLiteral("userId")).toInteger();
        connect(change, &QPushButton::clicked, this, [this, userId, frozen]() { emit statusChangeRequested(userId, !frozen); });
        m_table->setCellWidget(row, 6, change);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
