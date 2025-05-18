#include "DeviceController.h"

DeviceController::DeviceController(QObject *parent)
    : QObject{parent}
{
    connect(&_socket, &QTcpSocket::connected, this, &DeviceController::connected);
    connect(&_socket, &QTcpSocket::disconnected, this, &DeviceController::disconnected);
    connect(&_socket, &QTcpSocket::errorOccurred, this, &DeviceController::errorOccurred);
    connect(&_socket, &QTcpSocket::stateChanged, this, &DeviceController::socket_stateChanged);
    connect(&_socket, &QTcpSocket::readyRead, this, &DeviceController::socket_readyRead);
}

void DeviceController::connectToDevice(QString ip, int port)
{
    if (_socket.isOpen()) {
        if (ip == _ip && port == _port) {
            return;
        }
        _socket.close();
    }
    _ip = ip;
    _port = port;
    _socket.connectToHost(_ip, _port);

}

void DeviceController::disconnect()
{
    _socket.close();
}

QAbstractSocket::SocketState DeviceController::state()
{
    return _socket.state();
}

bool DeviceController::isConnected()
{
    return _socket.state() == QAbstractSocket::ConnectedState;
}

void DeviceController::send(QString message)
{
    _socket.write(message.toUtf8());
}

void DeviceController::send(const QByteArray& data)
{
    if (_socket.state() == QAbstractSocket::ConnectedState) {
        qDebug() << "Sending data as QByteArray, size:" << data.size() << ", first few bytes:" << data.left(16).toHex();
        _socket.write(data);
        _socket.flush();
    }
}

void DeviceController::send(const QVariant& data)
{
    if (_socket.state() == QAbstractSocket::ConnectedState) {
        if (data.canConvert<QString>()) {
            qDebug() << "Sending data as QString:" << data.toString().left(100);
            _socket.write(data.toString().toUtf8() + "\n");
        } else if (data.canConvert<QByteArray>()) {
            QByteArray byteData = data.toByteArray();
            qDebug() << "Sending data as QByteArray, size:" << byteData.size() << ", first few bytes:" << byteData.left(16).toHex();
            _socket.write(byteData);
        } else {
            qDebug() << "Unknown data type in send:" << data;
        }
        _socket.flush();
    }
}

void DeviceController::socket_stateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::UnconnectedState) {
        _socket.close();
    }
    emit stateChanged(state);
}

void DeviceController::socket_readyRead()
{
    auto data = _socket.readAll();
    emit dataReady(data);
}
