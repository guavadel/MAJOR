#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QDebug>
#include <QFile>
#include <QPixmap>
#include <QByteArray>
#include <QLabel>
#include <QImage>

class MyTCPServer : public QObject
{
    Q_OBJECT

public:
    explicit MyTCPServer(int port, QObject *parent = nullptr);
    bool isStarted() const;
    void sendToAll(QString message);
    void sendToClient(QTcpSocket *socket, const QString &message); // New method
    QList<QTcpSocket*> getConnectedClients() const { return _socketsList; }
    void disconnectClient(QTcpSocket* socket);

signals:
    void newClientConnected();
    void clientDisconnect();
    void dataReceived(QString data);
    void telemetryReceived(float latitude, float longitude, float altitude);
    void imageReceived(const QImage& image); // New signal for image reception
    void clientListUpdated();

private slots:
    void on_client_connecting();
    void clientDisconnected();
    void clientDataReady();
    void clientDataReadyImage(QTcpSocket* socket, QByteArray& buffer);

private:
    QTcpServer* _server;
    bool _isStarted;
    QList<QTcpSocket*> _socketsList;
    void processImageData(QTcpSocket* socket, QByteArray& buffer);
    static const quint32 IMAGE_HEADER = 0xA1B2C3D4;
    QMap<QTcpSocket*, QByteArray> socketBuffers; // Per-socket buffer for image data
};

#endif // MYTCPSERVER_H
