#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include "mainwindow.h"
#include "setupwizard.h"
#include "databasemanager.h"
#include <QDir>

int main(int argc, char *argv[])
{
    // Force X11 (xcb) even on Wayland to allow absolute positioning
    qputenv("QT_QPA_PLATFORM", "xcb");
    
    // Initialize Qt Application
    QApplication a(argc, argv);
    a.setApplicationName("com.toice.app");
    a.setOrganizationName("Toice");
    a.setDesktopFileName("com.toice.app.desktop"); // Canonical filename for KDE/Wayland
    a.setWindowIcon(QIcon(":/assets/logo.svg")); 
    a.setQuitOnLastWindowClosed(false); 

    // Single Instance Guard
    const QString serverName = "toice_local_server";
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) {
        qDebug() << "Another instance is running. Sending command.";
        
        bool toggle = false;
        for (int i = 1; i < argc; ++i) {
            if (QString(argv[i]) == "--toggle") {
                toggle = true;
                break;
            }
        }
        
        if (toggle) {
            socket.write("TOGGLE");
        } else {
            socket.write("SHOW_UI");
        }
        
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0; // Exit this instance
    }

    // 1. Initialize Database
    if (!DatabaseManager::instance().init()) {
        return 1;
    }

    // 2. Check for first-run (no model)
    if (DatabaseManager::instance().getSetting("model_path").isEmpty()) {
        SetupWizard wizard;
        if (wizard.exec() != QDialog::Accepted) {
            return 0; // User cancelled setup
        }
    }

    // Determine launch mode
    bool startInBackground = false;
    bool startWithToggle = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "--background") {
            startInBackground = true;
        } else if (arg == "--toggle") {
            startWithToggle = true;
            startInBackground = true; // Don't show main window if toggling
        }
    }

    // Create Main Window
    MainWindow w;

    if (startWithToggle) {
        // Use a small delay to ensure everything is ready
        QTimer::singleShot(100, &w, &MainWindow::toggleTranscription);
    }
    
    // Setup Local Server
    QLocalServer server;
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [&, client]() {
             QByteArray data = client->readAll();
             if (data == "SHOW_UI") {
                 qDebug() << "Received SHOW_UI command.";
                 w.showMainWindow();
                 w.raise();
                 w.activateWindow();
             } else if (data == "TOGGLE") {
                 qDebug() << "Received TOGGLE command.";
                 w.toggleTranscription();
             }
        });
    });
    
    if (!server.listen(serverName)) {
        // If listen fails (e.g. stale socket file), try removing it and listening again
        QLocalServer::removeServer(serverName);
        if (!server.listen(serverName)) {
            qCritical() << "Unable to start local server.";
        }
    }

    // Show or Hide based on launch mode
    if (startInBackground) {
        qDebug() << "Starting in background mode.";
    } else {
        w.show();
    }
    
    return a.exec();
}
