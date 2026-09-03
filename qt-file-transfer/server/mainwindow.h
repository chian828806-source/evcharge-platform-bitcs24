#pragma once
#include <QMainWindow>
#include <QTcpServer>
#include "filetransfer.h"
namespace Ui{class MainWindow;}
class MainWindow:public QMainWindow{Q_OBJECT public:explicit MainWindow(QWidget *parent = nullptr);~MainWindow();private slots:void listen();void newConnection();void log(const QString&);private:Ui::MainWindow*ui;QTcpServer*m_server;QString m_dir;QList<FileTransfer*>m_transfers;};
