#include "GUI.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QDir>
#include <QLabel>
#include <QImage>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Streamers/IGTLinkStreamer.hpp>
#include "OpenIGTLinkClient.hpp"
#include <QMessageBox>
#include <QElapsedTimer>
#include <QComboBox>


namespace fast {

GUI::GUI() {

    const int menuWidth = 300;

    mClient = OpenIGTLinkClient::New();
    mConnected = false;
    mRecordTimer = new QElapsedTimer;

    // Create view layout
    QVBoxLayout* viewLayout = new QVBoxLayout;
    QHBoxLayout* selectStreamLayout = new QHBoxLayout;
    viewLayout->addLayout(selectStreamLayout);

    QLabel* selectStreamLabel = new QLabel;
    selectStreamLabel->setText("Active input stream: ");
    selectStreamLabel->setFixedHeight(30);
    selectStreamLabel->setFixedWidth(150);
    selectStreamLayout->addWidget(selectStreamLabel);

    mSelectStream = new QComboBox;
    selectStreamLayout->addWidget(mSelectStream);
    QObject::connect(mSelectStream, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), std::bind(&GUI::selectStream, this));

    View* view = createView();
    view->set2DMode();
    view->setBackgroundColor(Color::Black());
    setWidth(1280);
    setHeight(768);
    setTitle("FAST - OpenIGTLink Client");
    viewLayout->addWidget(view);

    // First create the menu layout
    QVBoxLayout* menuLayout = new QVBoxLayout;

    // Menu items should be aligned to the top
    menuLayout->setAlignment(Qt::AlignTop);

    // Logo
    QImage* image = new QImage;
    image->load((Config::getDocumentationPath() + "images/FAST_logo_square.png").c_str());
    QLabel* logo = new QLabel;
    logo->setPixmap(QPixmap::fromImage(image->scaled(300, (300.0f/image->width())*image->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    logo->adjustSize();
    menuLayout->addWidget(logo);

    // Title label
    QLabel* title = new QLabel;
    title->setText("<div style=\"text-align: center; font-weight: bold; font-size: 24px;\">OpenIGTLink<br>Client</div>");
    menuLayout->addWidget(title);

    // Quit button
    QPushButton* quitButton = new QPushButton;
    quitButton->setText("Quit (q)");
    quitButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
    quitButton->setFixedWidth(menuWidth);
    menuLayout->addWidget(quitButton);

    // Connect the clicked signal of the quit button to the stop method for the window
    QObject::connect(quitButton, &QPushButton::clicked, std::bind(&Window::stop, this));

    // Address and port
    QLabel* addressLabel = new QLabel;
    addressLabel->setText("Server address");
    menuLayout->addWidget(addressLabel);

    mAddress = new QLineEdit;
    mAddress->setText("localhost");
    mAddress->setFixedWidth(menuWidth);
    menuLayout->addWidget(mAddress);

    QLabel* portLabel = new QLabel;
    portLabel->setText("Server port");
    menuLayout->addWidget(portLabel);

    mPort = new QLineEdit;
    mPort->setText("18944");
    mPort->setFixedWidth(menuWidth);
    menuLayout->addWidget(mPort);

    connectButton = new QPushButton;
    connectButton->setText("Connect");
    connectButton->setFixedWidth(menuWidth);
    connectButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
    menuLayout->addWidget(connectButton);

    QObject::connect(connectButton, &QPushButton::clicked, std::bind(&GUI::connect, this));

    QLabel* storageDirLabel = new QLabel;
    storageDirLabel->setText("Storage directory");
    menuLayout->addWidget(storageDirLabel);

    storageDir = new QLineEdit;
    storageDir->setText(QDir::homePath() + QDir::separator() + QString("FAST_Recordings"));
    storageDir->setFixedWidth(menuWidth);
    menuLayout->addWidget(storageDir);

    recordButton = new QPushButton;
    recordButton->setText("Record (spacebar)");
    recordButton->setFixedWidth(menuWidth);
    recordButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
    menuLayout->addWidget(recordButton);

    recordingInformation = new QLabel;
    recordingInformation->setFixedWidth(menuWidth);
    recordingInformation->setStyleSheet("QLabel { font-size: 14px; }");
    menuLayout->addWidget(recordingInformation);

    QObject::connect(recordButton, &QPushButton::clicked, std::bind(&GUI::record, this));

    // Add menu and view to main layout
    QHBoxLayout* layout = new QHBoxLayout;
    layout->addLayout(menuLayout);
    layout->addLayout(viewLayout);

    mWidget->setLayout(layout);

    // Update messages frequently
    QTimer* timer = new QTimer(this);
    timer->start(1000/5); // in milliseconds
    timer->setSingleShot(false);
    QObject::connect(timer, &QTimer::timeout, std::bind(&GUI::updateMessages, this));

    // Update streams info now and then
    QTimer* timer2 = new QTimer(this);
    timer2->start(1000); // in milliseconds
    timer2->setSingleShot(false);
    QObject::connect(timer2, &QTimer::timeout, std::bind(&GUI::refreshStreams, this));

    connectButton->setFocus();
}

void GUI::selectStream() {
    std::string streamName = mStreamNames[mSelectStream->currentIndex()];
    reportInfo() << "Changing to " << streamName << " stream " << reportEnd();

    // Stop computation thread before removing renderers
    stopComputationThread();
    getView(0)->removeAllRenderers();
    mStreamer->stop(); // This should probably block until it has stopped
    reportInfo() << "Disconnected" << reportEnd();
    startComputationThread();

    reportInfo() << "Trying to connect..." << reportEnd();
    mStreamer = IGTLinkStreamer::New();
    mStreamer->setConnectionAddress(mAddress->text().toStdString());
    mStreamer->setConnectionPort(std::stoi(mPort->text().toStdString()));
    mClient->setInputConnection(mStreamer->getOutputPort<Image>(streamName));
    try {
        mStreamer->update();
    } catch(Exception &e) {
        QMessageBox* message = new QMessageBox;
        message->setWindowTitle("Error");
        message->setText(e.what());
        message->show();
        return;
    }
    reportInfo() << "Connected to OpenIGTLink server." << reportEnd();

    ImageRenderer::pointer renderer = ImageRenderer::New();
    renderer->addInputConnection(mClient->getOutputPort());

    getView(0)->addRenderer(renderer);
    // This causes seg fault for some reason... wrong thread maybe?
    //getView(0)->reinitialize();

    recordButton->setFocus();
    //refreshStreams();
}

void GUI::connect() {
    if(mConnected) {
        // Stop computation thread before removing renderers
        stopComputationThread();
        getView(0)->removeAllRenderers();
        mStreamer->stop(); // This should probably block until it has stopped
        reportInfo() << "Disconnected" << reportEnd();
        startComputationThread();

        connectButton->setText("Connect");
        connectButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
        mAddress->setDisabled(false);
        mPort->setDisabled(false);
        mConnected = false;
        connectButton->setFocus();
        mSelectStream->clear();
    } else {
        reportInfo() << "Trying to connect..." << reportEnd();
        mStreamer = IGTLinkStreamer::New();
        mStreamer->setConnectionAddress(mAddress->text().toStdString());
        mStreamer->setConnectionPort(std::stoi(mPort->text().toStdString()));
        mClient->setInputConnection(mStreamer->getOutputPort());
        try {
            mStreamer->update();
        } catch(Exception &e) {
            QMessageBox* message = new QMessageBox;
            message->setWindowTitle("Error");
            message->setText(e.what());
            message->show();
            return;
        }
        reportInfo() << "Connected to OpenIGTLink server." << reportEnd();

        ImageRenderer::pointer renderer = ImageRenderer::New();
        renderer->addInputConnection(mClient->getOutputPort());

        getView(0)->addRenderer(renderer);
        getView(0)->reinitialize();

        connectButton->setText("Disconnect");
        connectButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        mAddress->setDisabled(true);
        mPort->setDisabled(true);
        mConnected = true;
        recordButton->setFocus();
        //refreshStreams();
    }
}

void GUI::refreshStreams() {// Get all stream names and put in select box
    if(mConnected) {
        int activeIndex = 0;
        std::vector<std::string> activeStreams = mStreamer->getActiveImageStreamNames();
        int counter = 0;
        mStreamNames.clear();
        mSelectStream->clear();
        for(std::string streamName : mStreamer->getImageStreamNames()) {
            mSelectStream->addItem((streamName + " (" + mStreamer->getStreamDescription(streamName) + ")").c_str());
            mStreamNames.push_back(streamName);
            if(streamName == activeStreams[0]) {
                activeIndex = counter;
            }
            ++counter;
        }
        mSelectStream->setCurrentIndex(activeIndex);
    }
}

void GUI::record() {
    if(!mConnected) {
        // Can't record if not connected
        QMessageBox* message = new QMessageBox;
        message->setWindowTitle("Error");
        message->setText("You have to connect to a server before recording.");
        message->show();
        return;
    }
    bool recording = mClient->toggleRecord(storageDir->text().toStdString());
    if(recording) {
        mRecordTimer->start();
        std::string msg = "Recording to: " + mClient->getRecordingName();
        recordingInformation->setText(msg.c_str());
        // Start
        recordButton->setText("Stop recording (spacebar)");
        recordButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        storageDir->setDisabled(true);
        recordButton->setFocus();
    } else {
        // Stop
        std::string msg = "Recording saved in: " + mClient->getRecordingName() + "\n";
        msg += std::to_string(mClient->getFramesStored()) + " frames stored\n";
        msg += format("%.1f seconds", (float)mRecordTimer->elapsed()/1000.0f);
        recordingInformation->setText(msg.c_str());
        recordButton->setText("Record (spacebar)");
        recordButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
        storageDir->setDisabled(false);
        recordButton->setFocus();
    }
}

void GUI::updateMessages() {
    if(mClient->isRecording()) {
        std::string msg = "Recording to: " + mClient->getRecordingName() + "\n";
        msg += std::to_string(mClient->getFramesStored()) + " frames stored\n";
        msg += format("%.1f seconds", (float)mRecordTimer->elapsed()/1000.0f);
        recordingInformation->setText(msg.c_str());
    }
}

}
