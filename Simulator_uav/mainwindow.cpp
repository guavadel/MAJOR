#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicecontroller.h"

#include <QMetaEnum>
#include <QMessageBox>
#include <QJsonObject>
#include <QBuffer>
#include <QPixmap>
#include <QImage>
#include <QtNetwork/QTcpSocket>
#include <QTimer>
#include <QString>
#include <cstdlib>
#include <ctime>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QThread>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>

const QString IMAGE_DIR = ":/images";  // <- Change this to your folder path


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setDeviceContoller();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    currentPosition.latitude = 28.6139;
    currentPosition.longitude = 77.2090;
    currentPosition.altitude = 300.0f;
    telemetryTimer = new QTimer(this);
    connect(telemetryTimer, &QTimer::timeout, this, &MainWindow::sendTelemetryData);
    telemetryTimer->start(2000);
    imageSendTimer = new QTimer(this);
    connect(imageSendTimer, &QTimer::timeout, this, &MainWindow::sendNextImage);


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_lnIPAddress_textChanged(const QString &arg1)
{
    QString state = "0";
    if (arg1 == "...") {
        state = "";
    } else {
        QHostAddress address(arg1);
        if (QAbstractSocket::IPv4Protocol == address.protocol()) {
            state = "1";
        }
    }
    ui->lnIPAddress->setProperty("state", state);
    style()->polish(ui->lnIPAddress);
}


void MainWindow::on_btnConnect_clicked()
{
    if (_controller.isConnected()) {
        _controller.disconnect();
    } else {
        auto ip = ui->lnIPAddress->text();
        auto port = ui->spnPort->value();
        _controller.connectToDevice(ip, port);
    }
}

void MainWindow::device_connected()
{
    ui->lstConsole->addItem("Connected To Device");
    ui->btnConnect->setText("Disconnect");
    ui->grpSendData->setEnabled(true);
}

void MainWindow::device_disconnected()
{
    ui->lstConsole->addItem("Disconnected from Device");
    ui->btnConnect->setText("Connect");
    ui->grpSendData->setEnabled(false);
}

void MainWindow::device_stateChanged(QAbstractSocket::SocketState state)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<QAbstractSocket::SocketState >();
    ui->lstConsole->addItem(metaEnum.valueToKey(state));
}

void MainWindow::device_errorOccurred(QAbstractSocket::SocketError error)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<QAbstractSocket::SocketError >();
    ui->lstConsole->addItem(metaEnum.valueToKey(error));

}

void MainWindow::device_dataReady(QByteArray data)
{
    ui->lstConsole->addItem(QString(data));
}

void MainWindow::setDeviceContoller()
{
    connect(&_controller, &DeviceController::connected, this, &MainWindow::device_connected);
    connect(&_controller, &DeviceController::disconnected, this, &MainWindow::device_disconnected);
    connect(&_controller, &DeviceController::stateChanged, this, &MainWindow::device_stateChanged);
    connect(&_controller, &DeviceController::errorOccurred, this, &MainWindow::device_errorOccurred);
    connect(&_controller, &DeviceController::dataReady, this, &MainWindow::device_dataReady);
}


void MainWindow::on_btnSend_clicked()
{
    auto message = ui->lnMessage->text().trimmed();
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);

    quint32 headerValue = 0xB1B2B3B4;
    out << headerValue;
    out << qint32(message.size());
    packet.append(message.toUtf8());
    out.setByteOrder(QDataStream::BigEndian);   // Explicitly set byte order

    qDebug() << "Client sending text message, total packet size:" << packet.size();
    _controller.send(packet);
}

void MainWindow::on_sendImageButton_clicked()
{
    QImage image(":/images/Agri.jpeg");
    if (image.isNull()) {
        ui->lstConsole->addItem("Failed to load image: Agri.jpeg");
        qDebug() << "Failed to load image.";
        return;
    }

    // Convert QImage to QByteArray explicitly
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");
    buffer.close();

    // Create packet with header and size
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15); // Ensure consistent version
    quint32 headerValue = 0xA1B2C3D4;
    out << headerValue; // Write header
    out << qint32(imageData.size()); // Write size of image data
    packet.append(imageData); // Append the image data directly

    qDebug() << "Client sending image, total packet size:" << packet.size();
    qDebug() << "Header should be a1b2c3d4, actual:" << QByteArray((const char*)&headerValue, sizeof(quint32)).toHex();
    qDebug() << "First few bytes of packet:" << packet.left(16).toHex();
    qDebug() << "Image data size:" << imageData.size();

    // Send the serialized data
    _controller.send(packet);

    // Ensure socket flushes
    QCoreApplication::processEvents();
    QThread::msleep(50);

    ui->lstConsole->addItem("Image sent to server.");
}
void MainWindow::sendTelemetryData()
{
    // Simulate real-time changing values
    currentPosition.latitude += ((std::rand() % 100) - 50) * 0.0001;
    currentPosition.longitude += ((std::rand() % 100) - 50) * 0.0001;
    currentPosition.altitude += ((std::rand() % 20) - 10) * 0.1;

    QString telemetry = QString("Latitude: %1, Longitude: %2, Altitude: %3")
                            .arg(currentPosition.latitude, 0, 'f', 6)
                            .arg(currentPosition.longitude, 0, 'f', 6)
                            .arg(currentPosition.altitude, 0, 'f', 2);

    ui->latLabel->setText(QString("Latitude: %1").arg(currentPosition.latitude, 0, 'f', 6));
    ui->lonLabel->setText(QString("Longitude: %1").arg(currentPosition.longitude, 0, 'f', 6));
    ui->altLabel->setText(QString("Altitude: %1").arg(currentPosition.altitude, 0, 'f', 2));

    // Prepare data with header and size
    QByteArray payload = telemetry.toUtf8();
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 header = 0xB1B2B3B4;
    qint32 size = payload.size();

    stream << header << size;
    packet.append(payload);

    _controller.send(packet);
}


void MainWindow::on_sendTelemetryButton_clicked()
{
    if (telemetryTimer->isActive()) {
        telemetryTimer->stop();
        ui->lstConsole->addItem("Telemetry stopped.");
        ui->sendTelemetryButton->setText("Start sending Telemetry data");
        ui->sendTelemetryButton->setStyleSheet("background-color: green; color: white; padding: 5px;");
    } else {
        telemetryTimer->start(2000);
        ui->lstConsole->addItem("Telemetry started.");
        ui->sendTelemetryButton->setText("Stop sending Telemetry data");
        ui->sendTelemetryButton->setStyleSheet("background-color: red; color: white; padding: 5px;");
    }
}

void MainWindow::sendNextImage()
{
    if (imageList.isEmpty() || !_controller.isConnected()) {
        ui->lstConsole->addItem("No images or not connected.");
        return;
    }
    currentImageIndex = currentImageIndex % imageList.size();
    QString imagePath = imageList[currentImageIndex++];
    QImage image(imagePath);
    if (image.isNull()) {
        ui->lstConsole->addItem("Failed to load: " + imagePath);
        return;
    }

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");
    buffer.close();

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    quint32 headerValue = 0xA1B2C3D4;
    out << headerValue;
    out << qint32(imageData.size());
    packet.append(imageData);

    _controller.send(packet);
    ui->lstConsole->addItem(QString("Sent image %1").arg(imagePath));
}


void MainWindow::on_imagesLabel_clicked()
{
    if (imageSendTimer->isActive()) {
        imageSendTimer->stop();
        ui->imagesLabel->setText("Start Image Sending");
        ui->imagesLabel->setStyleSheet("background-color: green; color: white; padding: 5px;");
        ui->lstConsole->addItem("Image sending stopped.");
        return;
    }

    QDir dir(IMAGE_DIR);
    QStringList filters = {"*.jpg", "*.jpeg", "*.png"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);

    imageList.clear();
    for (const QFileInfo& fileInfo : files) {
        imageList.append(fileInfo.absoluteFilePath());
    }

    if (imageList.isEmpty()) {
        ui->lstConsole->addItem("No images found in directory: " + IMAGE_DIR);
        return;
    }

    currentImageIndex = 0;
    imageSendTimer->start(500);  // Send an image every 500ms

    ui->imagesLabel->setText("Stop Image Sending");
    ui->imagesLabel->setStyleSheet("background-color: red; color: white; padding: 5px;");
    ui->lstConsole->addItem(QString("Started sending %1 images from folder: %2")
                                .arg(imageList.size()).arg(IMAGE_DIR));
}


