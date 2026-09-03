#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QFile>

class FileTransfer : public QObject
{
    Q_OBJECT
public:
    explicit FileTransfer(QTcpSocket *socket, QObject *parent=nullptr);
    bool sendFile(const QString &path);
    void setReceiveDirectory(const QString &directory) { m_receiveDirectory=directory; }
signals:
    void logMessage(const QString &message);
    void progress(qint64 sent, qint64 total);
    void fileReceived(const QString &path);
    void failed(const QString &message);
    void finished();
private slots:
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onDisconnected();
private:
    bool parseHeader();
    void resetReceive();
    QTcpSocket *m_socket;
    QFile m_file;
    QByteArray m_buffer;
    QString m_name;
    qint64 m_size=0, m_received=0, m_sent=0;
    bool m_receiving=false;
    QByteArray m_header;
    QString m_receiveDirectory;
};
