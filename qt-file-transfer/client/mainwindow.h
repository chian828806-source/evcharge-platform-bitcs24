#pragma once
#include <QMainWindow>
#include <QTcpSocket>
#include "filetransfer.h"
namespace Ui{class MainWindow;} class DropZone;
class MainWindow:public QMainWindow{Q_OBJECT public:explicit MainWindow(QWidget *parent = nullptr);~MainWindow(); private slots:void connectServer();void chooseAndSend();void sendDropped(const QString&);void log(const QString&);void fail(const QString&);void updateProgress(qint64,qint64);private:Ui::MainWindow*ui;QTcpSocket*m_socket;FileTransfer*m_transfer;};
