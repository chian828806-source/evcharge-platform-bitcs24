#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
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
signals:
    void refreshRequested();
    void trendRequested(int days);
private:
    QLabel *m_today = nullptr;
    QLabel *m_month = nullptr;
    QLabel *m_total = nullptr;
    QVBoxLayout *m_chartLayout = nullptr;
    QWidget *m_trendView = nullptr;
    QWidget *m_statusView = nullptr;
};

class PilePage : public QWidget
{
    Q_OBJECT
public:
    explicit PilePage(QWidget *parent = nullptr);
    void setPiles(const QJsonObject &data);
signals:
    void refreshRequested();
    void restartRequested(qint64 pileId);
private:
    QTableWidget *m_table = nullptr;
};

class StationPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationPage(QWidget *parent = nullptr);
    void setStations(const QJsonObject &data);
signals:
    void refreshRequested();
    void stationPilesRequested(qint64 stationId);
    void createRequested(const QJsonObject &payload);
private:
    void openCreateDialog();
    QTableWidget *m_table = nullptr;
};

class UserPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserPage(QWidget *parent = nullptr);
    void setUsers(const QJsonObject &data);
    QString phoneKeyword() const;
signals:
    void refreshRequested();
    void searchRequested(const QString &phoneKeyword);
    void statusChangeRequested(qint64 userId, bool freeze);
private:
    QLineEdit *m_search = nullptr;
    QTableWidget *m_table = nullptr;
};
