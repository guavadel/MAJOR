#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QFile> // Added for QFile
#include <QByteArray>
#include <QTcpServer>
#include <QTcpSocket>

class DeviceController : public QObject
{
    Q_OBJECT
public:
    explicit DeviceController(QObject *parent = nullptr);
    void connectToDevice(QString ip, int port);
    void disconnect();
    QAbstractSocket::SocketState state();
    bool isConnected();
    void send(QString message);
    void send(const QVariant& data); // Already supports both QString and QByteArray
    QTcpSocket* socket;
    void send(const QByteArray& data); // New overload for binary data


signals:
    void connected();
    void disconnected();
    void stateChanged(QAbstractSocket::SocketState);
    void errorOccurred(QAbstractSocket::SocketError);
    void dataReady(QByteArray data);

private slots:
    void socket_stateChanged(QAbstractSocket::SocketState state);
    void socket_readyRead();
private:
    QTcpSocket _socket;
    QString _ip;
    int _port;
};

#endif // DEVICECONTROLLER_H
