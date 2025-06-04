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
#include <QDesktopServices>
#include <QUrl>
#include <QGeoCoordinate>
#include <QLabel>
#include <QResizeEvent>
#include <QDateTime>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isBlinking(false) // Initialize isBlinking
{
    ui->setupUi(this);
    ui->gridLayout->setColumnStretch(0, 1);
    ui->gridLayout->setRowStretch(0, 1);
    ui->gridLayout->setColumnStretch(1, 1);
    ui->gridLayout->setRowStretch(1, 1);
    _server = nullptr;
    statusTimer = new QTimer(this);
    blinkTimer = new QTimer(this); // Initialize blinkTimer
    startTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    lastCommand = "None";
    serverStartTime = QDateTime(); // Initialize as invalid

    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    connect(blinkTimer, &QTimer::timeout, this, [this]() {
        isBlinking = !isBlinking;
        if (ui->lblConnectionStatus) {
            ui->lblConnectionStatus->setStyleSheet(isBlinking ? "background-color: yellow;" : (_server && _server->isStarted() ? "background-color: rgb(67, 135, 100);" : "background-color: rgb(255, 0, 0);"));
        }
        if (!isBlinking) blinkTimer->stop();
    });
    statusTimer->start(1000); // Update status bar every second
    loadSettings();
    setupLogging();
    updateStatusBar();
    qDebug() << "MainWindow initialized";
}

MainWindow::~MainWindow()
{
    if (logFile.isOpen()) {
        logFile.close();
    }
    if (statusTimer) {
        statusTimer->stop();
        delete statusTimer;
        statusTimer = nullptr;
    }
    if (blinkTimer) { // Clean up blinkTimer
        blinkTimer->stop();
        delete blinkTimer;
        blinkTimer = nullptr;
    }
    delete _server;
    delete ui;
}

void MainWindow::loadSettings()
{
    int port = settings.value("server/port", 12345).toInt();
    ui->spnServerPort->setValue(port);
    loggingEnabled = settings.value("logging/enabled", false).toBool();
    // Also load the log file path to ensure consistency
    QString logFilePath = settings.value("logging/filePath", QDir::homePath() + "/gcs.log").toString();
    settings.setValue("logging/filePath", logFilePath); // Ensure it’s set in case it was missing
}

void MainWindow::setupLogging()
{
    if (logFile.isOpen()) {
        logFile.close();
    }

    if (loggingEnabled) {
        QString logFilePath = settings.value("logging/filePath", QDir::homePath() + "/gcs.log").toString();
        logFile.setFileName(logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            logStream.setDevice(&logFile);
            logToFile("Logging started at " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        } else {
            qDebug() << "Failed to open log file:" << logFilePath;
            loggingEnabled = false;
        }
    }
}

void MainWindow::logToFile(const QString& message)
{
    if (loggingEnabled && logFile.isOpen()) {
        logStream << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "] " << message << "\n";
        logStream.flush();
    }
}

void MainWindow::startBlink(const QString& color)
{
    if (!ui->lblConnectionStatus) {
        qDebug() << "Error: lblConnectionStatus is null";
        logToFile("Error: lblConnectionStatus is null");
        return;
    }
    ui->lblConnectionStatus->setStyleSheet(QString("background-color: %1;").arg(color));
    isBlinking = false;
    blinkTimer->start(500);
}

void MainWindow::on_btnStartServer_clicked()
{
    qDebug() << "btnStartServer clicked";
    if (_server != nullptr) {
        delete _server;
        _server = nullptr;
        ui->btnStartServer->setText("Start Server");
        ui->lblConnectionStatus->setProperty("state", "0");
        style()->polish(ui->lblConnectionStatus);
        qDebug() << "Server stopped and deleted.";
        updateClientList(); // Clear the client list when server stops
        serverStartTime = QDateTime(); // Reset start time
        updateStatusBar();
        return;
    }

    auto port = ui->spnServerPort->value();
    _server = new MyTCPServer(port, this);
    serverStartTime = QDateTime::currentDateTime();
    connect(_server, &MyTCPServer::newClientConnected, this, &MainWindow::newClinetConnected, Qt::QueuedConnection);
    connect(_server, &MyTCPServer::dataReceived, this, &MainWindow::clientDataReceived, Qt::QueuedConnection);
    connect(_server, &MyTCPServer::clientDisconnect, this, &MainWindow::clientDisconnected, Qt::QueuedConnection);
    connect(_server, &MyTCPServer::telemetryReceived, this, &MainWindow::onTelemetryReceived, Qt::QueuedConnection);
    connect(_server, &MyTCPServer::imageReceived, this, &MainWindow::onImageReceived, Qt::QueuedConnection);
    connect(_server, &MyTCPServer::clientListUpdated, this, &MainWindow::updateClientList, Qt::QueuedConnection);
    qDebug() << "Server created and connected signals, port:" << port;

    if (_server->isStarted()) {
        ui->btnStartServer->setText("Stop Server");
        ui->lblConnectionStatus->setProperty("state", "1");
        style()->polish(ui->lblConnectionStatus);
        qDebug() << "Server started.";
        logToFile("Server started.");
        updateStatusBar();
    } else {
        qDebug() << "Failed to start server on port:" << port;
    }
}

void MainWindow::newClinetConnected()
{
    ui->lstConsole->addItem("New Client connected");
    qDebug() << "New client connected";
    startBlink("green"); // Trigger green blink on connect
    updateStatusBar();
}

void MainWindow::clientDisconnected()
{
    ui->lstConsole->addItem("Client Disconnected");
    qDebug() << "Client disconnected";
    startBlink("red"); // Trigger red blink on disconnect
    updateStatusBar();
}

void MainWindow::clientDataReceived(QString message)
{
    ui->lstConsole->addItem("Message: " + message);
    qDebug() << "Data received and logged to console:" << message;
}

void MainWindow::on_btnSendToAll_clicked()
{
    auto message = ui->lnMessage->text().trimmed();
    if (message.isEmpty()) {
        qDebug() << "Empty message, not sending";
        return;
    }
    if (_server && _server->isStarted()) {
        _server->sendToAll(message);
        lastCommand = message;
        logCommand(message);
        qDebug() << "Sent to all clients:" << message;
        updateStatusBar();
    } else {
        qDebug() << "Server not started, cannot send:" << message;
    }
}

void MainWindow::logCommand(const QString& command)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->commandHistoryList->addItem(QString("[%1] %2").arg(timestamp).arg(command));
    ui->commandHistoryList->scrollToBottom();
}

void MainWindow::updateStatusBar()
{
    QString clientsCount = QString("Clients: %1").arg(_server && _server->isStarted() ? _server->getConnectedClients().size() : 0);

    QString uptime = "Uptime: N/A";
    if (_server && _server->isStarted() && serverStartTime.isValid()) {
        qint64 seconds = serverStartTime.secsTo(QDateTime::currentDateTime());
        qint64 hours = seconds / 3600;
        seconds %= 3600;
        qint64 minutes = seconds / 60;
        seconds %= 60;
        uptime = QString("Uptime: %1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }

    QString lastCmd = QString("Last Command: %1").arg(lastCommand);

    ui->statusbar->showMessage(QString("%1 | %2 | %3").arg(clientsCount).arg(uptime).arg(lastCmd));
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
            // ui->lstConsole->addItem("Image received and displayed.");
            ui->imageLabel->repaint();
        }, Qt::QueuedConnection);
    } else {
        ui->lstConsole->addItem("Failed to process received image.");
    }
}

void MainWindow::setupGoogleMap(float latitude, float longitude)
{
    QLabel *mapLabel = ui->mapLabel;
    if (!mapLabel) {
        qDebug() << "Error: mapLabel not found in UI";
        return;
    }
    QString url = QString("https://maps.googleapis.com/maps/api/staticmap?center=%1,%2&scale=4&zoom=15&size=680x440&markers=color:red%7Clabel:U%7C%1,%2&key=AIzaSyBt3dvbCoW-WNjngOLuFsb1aqP6K4Bci_4")
                      .arg(latitude)
                      .arg(longitude);
    qDebug() << "Fetching Google Map with URL:" << url;

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
    manager->get(QNetworkRequest(QUrl(url)));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (!ui->mapLabel->pixmap().isNull()) {
        QPixmap pixmap = ui->mapLabel->pixmap();
        ui->mapLabel->setPixmap(pixmap.scaled(
            ui->mapLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            ));
    }
    if (!ui->imageLabel->pixmap().isNull()) {
        QPixmap pixmap = ui->imageLabel->pixmap();
        ui->imageLabel->setPixmap(pixmap.scaled(
            ui->imageLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            ));
    }
}

void MainWindow::updateClientList()
{
    ui->clientTable->clearContents();
    ui->clientTable->setRowCount(0);
    clientSockets.clear();

    if (!_server || !_server->isStarted()) {
        return;
    }

    auto clients = _server->getConnectedClients();
    ui->clientTable->setRowCount(clients.size());

    for (int i = 0; i < clients.size(); ++i) {
        auto socket = clients[i];
        clientSockets[i] = socket;

        QTableWidgetItem *ipItem = new QTableWidgetItem(socket->peerAddress().toString());
        QTableWidgetItem *portItem = new QTableWidgetItem(QString::number(socket->peerPort()));
        QTableWidgetItem *statusItem = new QTableWidgetItem("Connected");

        ipItem->setFlags(ipItem->flags() & ~Qt::ItemIsEditable);
        portItem->setFlags(portItem->flags() & ~Qt::ItemIsEditable);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);

        ui->clientTable->setItem(i, 0, ipItem);
        ui->clientTable->setItem(i, 1, portItem);
        ui->clientTable->setItem(i, 2, statusItem);
    }
    updateStatusBar();
}

void MainWindow::on_btnDisconnectClient_clicked()
{
    int row = ui->clientTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a client to disconnect.");
        return;
    }

    if (clientSockets.contains(row)) {
        auto socket = clientSockets[row];
        _server->disconnectClient(socket);
        updateClientList();
    }
}

void MainWindow::on_btnSendToSelected_clicked()
{
    auto message = ui->lnMessage->text().trimmed();
    if (message.isEmpty()) {
        qDebug() << "Empty message, not sending";
        return;
    }

    int row = ui->clientTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a client to send the message to.");
        return;
    }

    if (_server && _server->isStarted() && clientSockets.contains(row)) {
        auto socket = clientSockets[row];
        _server->sendToClient(socket, message);
        lastCommand = message;
        logCommand(QString("To %1:%2: %3").arg(socket->peerAddress().toString()).arg(socket->peerPort()).arg(message));
        qDebug() << "Sent to selected client:" << message;
        updateStatusBar();
    } else {
        qDebug() << "Server not started or invalid client selected";
    }
}

void MainWindow::on_btnSettings_clicked()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        loadSettings();
        setupLogging();
        if (_server && _server->isStarted()) {
            QMessageBox::information(this, "Restart Required", "Please restart the server to apply the new port settings.");
        }
    }
}
