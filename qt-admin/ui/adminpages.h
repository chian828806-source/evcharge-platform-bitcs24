#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);
    void setRevenueSummary(const QJsonObject &data);
    void setRevenueTrend(const QJsonObject &data);
    void setPileStatusSummary(const QJsonObject &data);
    void setWarnings(const QJsonObject &data);
signals:
    void refreshRequested();
    void trendRequested(int days);
    void warningRequested(const QString &horizon);
private:
    QLabel *m_today = nullptr;
    QLabel *m_month = nullptr;
    QLabel *m_total = nullptr;
    QVBoxLayout *m_chartLayout = nullptr;
    QVBoxLayout *m_statusLayout = nullptr;
    QWidget *m_trendView = nullptr;
    QWidget *m_statusView = nullptr;
    QTableWidget *m_warningTable = nullptr;
};

class PilePage : public QWidget
{
    Q_OBJECT
public:
    explicit PilePage(QWidget *parent = nullptr);
    void setPiles(const QJsonObject &data);
    void setActionBusy(bool busy);
    void setStationFilterLabel(const QString &stationName);
signals:
    void refreshRequested();
    void restartRequested(qint64 pileId);
private:
    QTableWidget *m_table = nullptr;
    QLabel *m_filterHint = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QJsonArray m_piles;
    bool m_actionBusy = false;
    void applyFilter();
};

class StationPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationPage(QWidget *parent = nullptr);
    void setStations(const QJsonObject &data);
    void setPileDetails(const QJsonArray &piles);
    void setPileDetailsLoading();
    void setPileDetailsError(const QString &message);
    void setCreateBusy(bool busy);
signals:
    void refreshRequested();
    void stationPilesRequested(qint64 stationId);
    void createRequested(const QJsonObject &payload);
private:
    void openCreateDialog();
    QTableWidget *m_table = nullptr;
    QTableWidget *m_pileDetail = nullptr;
    QLabel *m_pileDetailTitle = nullptr;
    QPushButton *m_create = nullptr;
};

class UserPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserPage(QWidget *parent = nullptr);
    void setUsers(const QJsonObject &data);
    QString phoneKeyword() const;
    void setActionBusy(bool busy);
signals:
    void refreshRequested();
    void searchRequested(const QString &phoneKeyword);
    void statusChangeRequested(qint64 userId, bool freeze);
private:
    QLineEdit *m_search = nullptr;
    QTableWidget *m_table = nullptr;
    bool m_actionBusy = false;
};
