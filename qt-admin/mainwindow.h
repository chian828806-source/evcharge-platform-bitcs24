#pragma once

#include <QMainWindow>
#include <QJsonObject>

class AdminSocketClient;
class QLabel;
class QLineEdit;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void submitLogin();
    void handleResponse(const QJsonObject &response);

    AdminSocketClient *m_client = nullptr;
    QLineEdit *m_host = nullptr;
    QLineEdit *m_port = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QPushButton *m_login = nullptr;
    QLabel *m_status = nullptr;
    QString m_loginRequestId;
};
