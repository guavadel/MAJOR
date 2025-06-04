#include "MyTCPServer.h"
#include <QDataStream>

MyTCPServer::MyTCPServer(int port, QObject *parent)
    : QObject(parent), _isStarted(false)
{
    _server = new QTcpServer(this);
    connect(_server, &QTcpServer::newConnection, this, &MyTCPServer::on_client_connecting);
    _isStarted = _server->listen(QHostAddress::Any, port);
    if (!_isStarted) {
        qDebug() << "Server could not start";
    } else {
        qDebug() << "Server started on port" << port;
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
    emit clientListUpdated();
}

void MyTCPServer::clientDisconnected()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        _socketsList.removeAll(socket);
        socketBuffers.remove(socket);
        socket->deleteLater();
        qDebug() << "Client disconnected:" << socket->peerAddress().toString();
    }
    emit clientDisconnect();
    emit clientListUpdated();
}

void MyTCPServer::clientDataReady()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray& accumulatedBuffer = socketBuffers[socket];
    accumulatedBuffer.append(socket->readAll());

    while (true) {
        if (accumulatedBuffer.size() < static_cast<int>(sizeof(quint32) + sizeof(qint32))) {
            return;
        }

        QDataStream stream(accumulatedBuffer);
        stream.setVersion(QDataStream::Qt_5_15);
        stream.setByteOrder(QDataStream::BigEndian);

        quint32 header;
        qint32 size;
        stream >> header >> size;

        if (header == IMAGE_HEADER) { // Image data
            int totalSize = sizeof(quint32) + sizeof(qint32) + size;
            if (accumulatedBuffer.size() < totalSize)
                return;

            accumulatedBuffer.remove(0, sizeof(quint32) + sizeof(qint32));
            QByteArray imageData = accumulatedBuffer.left(size);
            accumulatedBuffer.remove(0, size);

            QImage image;
            if (image.loadFromData(imageData, "JPG")) {
                qDebug() << "✅ Image received. Size:" << image.size();
                emit imageReceived(image);
            } else {
                qDebug() << "❌ Failed to load image.";
            }
        }
        else if (header == 0xB1B2B3B4) { // Message or telemetry data
            int totalSize = sizeof(quint32) + sizeof(qint32) + size;
            if (accumulatedBuffer.size() < totalSize)
                return;

            accumulatedBuffer.remove(0, sizeof(quint32) + sizeof(qint32));
            QByteArray messageData = accumulatedBuffer.left(size);
            accumulatedBuffer.remove(0, size);

            QString message(messageData);

            bool isTelemetry = message.startsWith("Latitude:") &&
                               message.contains("Longitude:") &&
                               message.contains("Altitude:");

            if (!isTelemetry) {
                qDebug() << "📨 Message received:" << message;
                emit dataReceived(message);
            }

            if (isTelemetry) {
                QStringList parts = message.split(", ");
                if (parts.size() == 3) {
                    bool ok;
                    float lat = parts[0].split(": ")[1].toFloat(&ok);
                    if (ok) {
                        float lon = parts[1].split(": ")[1].toFloat(&ok);
                        if (ok) {
                            float alt = parts[2].split(": ")[1].toFloat(&ok);
                            if (ok) {
                                emit telemetryReceived(lat, lon, alt);
                            }
                        }
                    }
                }
            }
        }
        else {
            qDebug() << "❗ Unknown packet header:" << QString::number(header, 16);
            accumulatedBuffer.clear();
            return;
        }
    }
}

void MyTCPServer::clientDataReadyImage(QTcpSocket* socket, QByteArray& buffer)
{
    // Append incoming buffer to the accumulated buffer for this socket
    QByteArray& imageBuffer = socketBuffers[socket];
    imageBuffer.append(buffer);
    qDebug() << "Appended new data, total buffer size:" << imageBuffer.size();

    while (true) {
        // Ensure we have at least 8 bytes for the header (4) + image size (4)
        if (imageBuffer.size() < static_cast<int>(sizeof(quint32) + sizeof(qint32))) {
            qDebug() << "Not enough data for header and size, waiting for more...";
            return;
        }

        // Use a stream to read header and image size without removing from buffer
        QDataStream peekStream(imageBuffer);
        peekStream.setVersion(QDataStream::Qt_5_15);


        quint32 header;
        qint32 imageSize;
        peekStream >> header >> imageSize;

        // Validate header
        if (header != 0xA1B2C3D4) {
            qDebug() << "Invalid header:" << QString::number(header, 16);
            qDebug() << "Clearing buffer, first few bytes:" << imageBuffer.left(8).toHex();
            socketBuffers.remove(socket);
            return;
        }

        // Validate size
        if (imageSize <= 0) {
            qDebug() << "Invalid image size:" << imageSize << ", clearing buffer.";
            socketBuffers.remove(socket);
            return;
        }

        // Check if full image data has been received
        int totalRequired = sizeof(quint32) + sizeof(qint32) + imageSize;
        if (imageBuffer.size() < totalRequired) {
            qDebug() << "Incomplete image, waiting. Needed:" << totalRequired << ", have:" << imageBuffer.size();
            return;
        }

        // Remove header and size
        imageBuffer.remove(0, sizeof(quint32) + sizeof(qint32));

        // Extract the image data
        QByteArray imageData = imageBuffer.left(imageSize);
        imageBuffer.remove(0, imageSize);

        // Try to load image
        QImage image;
        if (image.loadFromData(imageData, "JPG")) {
            qDebug() << "✅ Image received successfully. Size:" << image.size();
            emit imageReceived(image);
        } else {
            qDebug() << "❌ Failed to load image. Size:" << imageData.size()
                << ", First bytes:" << imageData.left(16).toHex();
        }

        // Loop again to check if more images are in buffer
    }
}

bool MyTCPServer::isStarted() const
{
    return _isStarted;
}

void MyTCPServer::sendToAll(QString message)
{
    QByteArray data = message.toUtf8();
    for (auto socket : _socketsList) {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(data);
            socket->flush();
        }
    }
    qDebug() << "Sent to all clients:" << message;
}


void MyTCPServer::sendToClient(QTcpSocket *socket, const QString &message)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = message.toUtf8();
        socket->write(data);
        socket->flush();
        qDebug() << "Sent to client" << socket->peerAddress().toString() << ":" << message;
    } else {
        qDebug() << "Client not connected or invalid socket";
    }
}

void MyTCPServer::disconnectClient(QTcpSocket* socket)
{
    if (_socketsList.contains(socket)) {
        _socketsList.removeAll(socket);
        socketBuffers.remove(socket);
        socket->disconnectFromHost();
        socket->deleteLater();
        qDebug() << "Client disconnected by server:" << socket->peerAddress().toString();
        emit clientDisconnect();
        emit clientListUpdated();
    }
}
