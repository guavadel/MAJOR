#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "MyTCPServer.h"
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QJsonObject>
#include <QBuffer>
#include <QPixmap>
#include <QDebug>
#include <QThread>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>
#include <cstdlib>
#include <ctime>
#include <QDesktopServices>
#include <QUrl>
#include <QGeoCoordinate>
#include <QLabel>
#include <QResizeEvent>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->gridLayout->setColumnStretch(0, 1);  // First column stretches
    ui->gridLayout->setRowStretch(0, 1);     // First row stretches
    ui->gridLayout->setColumnStretch(1, 1);  // Second column stretches
    ui->gridLayout->setRowStretch(1, 1);
    _server = nullptr;
    qDebug() << "MainWindow initialized"; // Debug initialization

}

MainWindow::~MainWindow()
{
    delete ui;
    delete _server;
}

void MainWindow::on_btnStartServer_clicked()
{
    qDebug() << "btnStartServer clicked"; // Debug button click
    if (_server == nullptr) {
        auto port = ui->spnServerPort->value();
        _server = new MyTCPServer(port, this);
        connect(_server, &MyTCPServer::newClientConnected, this, &MainWindow::newClinetConnected);
        connect(_server, &MyTCPServer::dataReceived, this, &MainWindow::clientDataReceived);
        connect(_server, &MyTCPServer::clientDisconnect, this, &MainWindow::clientDisconnected);
        connect(_server, &MyTCPServer::telemetryReceived, this, &MainWindow::onTelemetryReceived);
        connect(_server, &MyTCPServer::imageReceived, this, &MainWindow::onImageReceived);
        qDebug() << "Server created and connected signals, port:" << port;
    }

    auto state = (_server->isStarted()) ? "1" : "0";
    ui->lblConnectionStatus->setProperty("state", state);
    style()->polish(ui->lblConnectionStatus);
    qDebug() << "Connection status set to:" << state;
}

void MainWindow::newClinetConnected()
{
    ui->lstConsole->addItem("New Client connected");
    qDebug() << "New client connected"; // Debug client connection
}

void MainWindow::clientDisconnected()
{
    ui->lstConsole->addItem("Client Disconnected");
    qDebug() << "Client disconnected"; // Debug client disconnect
}

void MainWindow::clientDataReceived(QString message)
{
    ui->lstConsole->addItem(message);
    qDebug() << "Data received and logged to console:" << message; // Debug data reception
}

void MainWindow::on_btnSendToAll_clicked()
{
    auto message = ui->lnMessage->text().trimmed();
    if (_server && _server->isStarted()) {
        _server->sendToAll(message);
        qDebug() << "Sent to all clients:" << message; // Debug send action
    } else {
        qDebug() << "Server not started, cannot send:" << message;
    }
}

void MainWindow::onTelemetryReceived(float latitude, float longitude, float altitude)
{
    qDebug() << "Telemetry received: lat=" << latitude << ", lon=" << longitude << ", alt=" << altitude;
    ui->latLabel->setText(QString("Latitude: %1").arg(latitude, 0, 'f', 6));
    ui->lonLabel->setText(QString("Longitude: %1").arg(longitude, 0, 'f', 6));
    ui->altLabel->setText(QString("Altitude: %1").arg(altitude, 0, 'f', 2));

    // Display the map:
    setupGoogleMap(latitude, longitude);
}

void MainWindow::onImageReceived(const QImage& image)
{
    if (!image.isNull()) {
        qDebug() << "Image received, dimensions:" << image.size();
        QMetaObject::invokeMethod(this, [this, image]() {
            ui->imageLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            ui->lstConsole->addItem("Image received and displayed.");
            ui->imageLabel->repaint();
        }, Qt::QueuedConnection);
    } else {
        ui->lstConsole->addItem("Failed to process received image.");
    }
}


void MainWindow::setupGoogleMap(float latitude, float longitude) {
    QLabel *mapLabel = ui->mapLabel;

    if (!mapLabel) {
        qDebug() << "Error: mapLabel not found in UI";
        return;
    }

    // Set the URL for Google Static Maps API
    QString url = QString("https://maps.googleapis.com/maps/api/staticmap?center=%1,%2&scale=4&zoom=15&size=680x440&markers=color:red%7Clabel:U%7C%1,%2&key=AIzaSyBt3dvbCoW-WNjngOLuFsb1aqP6K4Bci_4")
                      .arg(latitude)
                      .arg(longitude);
    qDebug() << "Fetching Google Map with URL:" << url;

    // Network request to fetch the map image
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    connect(manager, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                mapLabel->setPixmap(pixmap.scaled(mapLabel->size(),
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
                mapLabel->repaint();
                qDebug() << "Map image loaded successfully.";
            } else {
                mapLabel->setText("Failed to load map image");
                qDebug() << "Failed to load map image from data.";
            }
        } else {
            mapLabel->setText("Error loading map: " + reply->errorString());
            qDebug() << "Network error:" << reply->errorString();
        }
        reply->deleteLater();
    });

    // Trigger the request
    manager->get(QNetworkRequest(QUrl(url)));
}
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    // Resize mapLabel while keeping the aspect ratio
    if (!ui->mapLabel->pixmap().isNull()) {
        QPixmap pixmap = ui->mapLabel->pixmap();
        ui->mapLabel->setPixmap(pixmap.scaled(
            ui->mapLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            ));
    }

    // Resize imageLabel while keeping the aspect ratio
    if (!ui->imageLabel->pixmap().isNull()) {
        QPixmap pixmap = ui->imageLabel->pixmap();
        ui->imageLabel->setPixmap(pixmap.scaled(
            ui->imageLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            ));
    }
}

