#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QAbstractSocket>

class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    void connectToServer(const QString& host = "localhost", quint16 port = 5000);
    void disconnectFromHost();
    void sendMessage(const QString& message);
    bool isConnected() const;

signals:
    void messageReceived(const QString& message);
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorString);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onConnected();

private:
    QTcpSocket* socket_;
    QString host_;
    quint16 port_;
};

#endif // TCPCLIENT_H
