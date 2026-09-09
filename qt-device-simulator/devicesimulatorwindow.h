#pragma once
#include <QWidget>
#include <QJsonObject>
class QTcpSocket; class QTimer; class QLineEdit; class QLabel; class QTextEdit; class QPushButton;
class DeviceSimulatorWindow : public QWidget
{ Q_OBJECT
public: DeviceSimulatorWindow();
private slots: void connectDevice(); void disconnectDevice(); void connected(); void read(); void heartbeat(); void telemetry(); void injectFault(); void disconnectFault(); void reconnect();
private: void send(const QJsonObject &m); void log(const QString &s); void command(const QJsonObject &m);
 QTcpSocket *m_socket; QTimer *m_heartbeat,*m_telemetry,*m_reconnect; QLineEdit *m_host,*m_port,*m_pileId,*m_pileNo; QLabel *m_state,*m_values; QTextEdit *m_log; QString m_deviceState=QStringLiteral("AVAILABLE"),m_fault; double m_rated=60,m_energy=0; bool m_simulateCut=false; };
