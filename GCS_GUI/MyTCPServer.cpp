#include "MyTCPServer.h"

const quint32 MyTCPServer::IMAGE_HEADER;

MyTCPServer::MyTCPServer(int port, QObject *parent)
    : QObject(parent)
{
    _server = new QTcpServer(this);
    connect(_server, &QTcpServer::newConnection, this, &MyTCPServer::on_client_connecting);
    _isStarted = _server->listen(QHostAddress::Any, port);
    if (!_isStarted) {
        qDebug() << "Server could not start";
    } else {
        qDebug() << "Server started...";
    }
}

void MyTCPServer::on_client_connecting()
{
    qDebug() << "A client connected to server";
    auto socket = _server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &MyTCPServer::clientDataReady);
    connect(socket, &QTcpSocket::disconnected, this, &MyTCPServer::clientDisconnected);
    _socketsList.append(socket);
    socket->write("Welcome to this Server");
    emit newClientConnected();
}

void MyTCPServer::clientDisconnected()
{
    emit clientDisconnect();
}

void MyTCPServer::clientDataReady()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray buffer = socket->readAll();
    if (buffer.isEmpty()) {
        qDebug() << "Received empty buffer in clientDataReady, skipping";
        return;
    }

    qDebug() << "Received raw buffer, size:" << buffer.size() << ", first few bytes:" << buffer.left(16).toHex();

    // Check for telemetry first (string-based)
    QString data(buffer);
    if (data.startsWith("Latitude:")) {
        qDebug() << "Received telemetry data:" << data.left(100);
        emit dataReceived(data);
        if (data.contains("Longitude:") && data.contains("Altitude:")) {
            QStringList parts = data.split(", ");
            if (parts.size() == 3) {
                bool ok;
                float latitude = parts[0].split(": ")[1].toFloat(&ok);
                if (ok) {
                    float longitude = parts[1].split(": ")[1].toFloat(&ok);
                    if (ok) {
                        float altitude = parts[2].split(": ")[1].toFloat(&ok);
                        if (ok) {
                            qDebug() << "Parsed telemetry: lat=" << latitude << ", lon=" << longitude << ", alt=" << altitude;
                            emit telemetryReceived(latitude, longitude, altitude);
                        }
                    }
                }
            }
        }
        return;
    }

    // Handle image data
    clientDataReadyImage(socket, buffer);
}

void MyTCPServer::clientDataReadyImage(QTcpSocket* socket, QByteArray& buffer)
{
    // Append to the socket-specific buffer
    socketBuffers[socket].append(buffer);
    QByteArray& imageBuffer = socketBuffers[socket];

    qDebug() << "Processing image data, total buffer size:" << imageBuffer.size();

    // Ensure we have enough data for the header and size
    while (imageBuffer.size() >= sizeof(quint32) + sizeof(qint32)) {
        QDataStream in(&imageBuffer, QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_5_15); // Match client version
        quint32 header;
        in >> header;
        if (header != IMAGE_HEADER) {
            qDebug() << "Invalid image header detected:" << QString::number(header, 16) << ", clearing buffer";
            qDebug() << "Expected header: a1b2c3d4, received first 4 bytes:" << imageBuffer.left(4).toHex();
            imageBuffer.clear();
            socketBuffers.remove(socket);
            return;
        }

        qint32 imageDataSize;
        in >> imageDataSize;
        qDebug() << "Read image data size:" << imageDataSize;

        if (imageBuffer.size() - sizeof(quint32) - sizeof(qint32) < imageDataSize || imageDataSize <= 0) {
            qDebug() << "Insufficient data for image, waiting, needed:" << (sizeof(quint32) + sizeof(qint32) + imageDataSize) << ", have:" << imageBuffer.size();
            return;
        }

        // Remove header and size
        imageBuffer.remove(0, sizeof(quint32) + sizeof(qint32));

        QByteArray imageData = imageBuffer.left(imageDataSize);
        imageBuffer.remove(0, imageDataSize);

        QImage image;
        if (image.loadFromData(imageData, "JPG")) {
            qDebug() << "Successfully loaded QImage, dimensions:" << image.size();
            emit imageReceived(image);
            socketBuffers.remove(socket); // Clear buffer after success
        } else {
            qDebug() << "Failed to load QImage from data, size:" << imageData.size() << ", first few bytes:" << imageData.left(16).toHex();
            socketBuffers.remove(socket); // Clear buffer on failure
        }
    }
}


bool MyTCPServer::isStarted() const
{
    return _isStarted;
}

void MyTCPServer::sendToAll(QString message)
{
    foreach (auto socket, _socketsList) {
        socket->write(message.toUtf8());
    }
}
