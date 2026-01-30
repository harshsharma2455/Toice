#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include "databasemanager.h"

class SetupWizard : public QDialog {
    Q_OBJECT
public:
    SetupWizard(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Toice Setup");
        // FIX: Match dialog size to card size + padding
        setFixedSize(540, 600);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        
        setupUi();
    }

private:
    void setupUi() {
        setStyleSheet("background-color: #f4f4f5; font-family: 'Inter', sans-serif;"); // Zinc-100
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0); // Remove outer margin to let card float
        layout->setAlignment(Qt::AlignCenter);

        QWidget *card = new QWidget(this);
        card->setFixedSize(480, 520); // Fixed card size for better focus
        card->setObjectName("card");
        card->setStyleSheet(
            "QWidget#card { "
            "  background: #ffffff; "
            "  border: 1px solid #e4e4e7; "
            "  border-radius: 16px; " 
            "}"
        );
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);
        
        // Header
        QWidget *header = new QWidget();
        header->setFixedHeight(72);
        header->setStyleSheet("border-bottom: 1px solid #e4e4e7; background: #fafafa; border-top-left-radius: 16px; border-top-right-radius: 16px;");
        QHBoxLayout *hl = new QHBoxLayout(header);
        hl->setContentsMargins(32, 0, 32, 0);
        
        QLabel *title = new QLabel("Toice Setup", this);
        title->setStyleSheet("font-size: 20px; font-weight: 600; color: #18181b;");
        hl->addWidget(title);
        cardLayout->addWidget(header);
        
        // Body
        QWidget *body = new QWidget();
        QVBoxLayout *bl = new QVBoxLayout(body);
        bl->setContentsMargins(32, 32, 32, 32);
        bl->setSpacing(20);
        
        QLabel *desc = new QLabel("Choose a transcription model to begin. This will determine the accuracy and speed of the engine.", this);
        desc->setWordWrap(true);
        desc->setStyleSheet("color: #71717a; font-size: 14px; line-height: 1.5;");
        desc->setAlignment(Qt::AlignLeft);
        bl->addWidget(desc);

        modelSelector = new QComboBox(this);
        modelSelector->setStyleSheet(
            "QComboBox { background: white; color: #18181b; border: 1px solid #e4e4e7; border-radius: 8px; padding: 12px; font-size: 14px; font-weight: 500; }"
            "QComboBox::drop-down { border: none; width: 0px; }"
        );
        modelSelector->addItem("Base (Optimized & Fast)", "base");
        modelSelector->addItem("Small (Balanced)", "small");
        modelSelector->addItem("Medium (High Accuracy)", "medium");
        bl->addWidget(modelSelector);

        progress = new QProgressBar(this);
        progress->setStyleSheet(
            "QProgressBar { background: #f4f4f5; border: none; text-align: center; color: transparent; height: 6px; border-radius: 3px; }"
            "QProgressBar::chunk { background: #18181b; border-radius: 3px; }"
        );
        progress->setRange(0, 100);
        progress->setValue(0);
        progress->hide();
        bl->addWidget(progress);

        statusLabel = new QLabel("", this);
        statusLabel->setStyleSheet("color: #71717a; font-size: 12px; font-weight: 500;");
        statusLabel->setAlignment(Qt::AlignCenter);
        bl->addWidget(statusLabel);
        
        bl->addStretch();

        btnStart = new QPushButton("Proceed to Engine", this);
        btnStart->setStyleSheet(
            "QPushButton { background: #18181b; color: #ffffff; padding: 12px; font-weight: 600; border-radius: 8px; font-size: 14px; }"
            "QPushButton:hover { background: #27272a; }"
            "QPushButton:disabled { background: #e4e4e7; color: #a1a1aa; }"
        );
        connect(btnStart, &QPushButton::clicked, this, &SetupWizard::startDownload);
        bl->addWidget(btnStart);
        
        cardLayout->addWidget(body, 1);
        layout->addWidget(card);
    }

    void startDownload() {
        QString modelSize = modelSelector->currentData().toString();
        
        // CHECK FOR BUNDLED MODEL (Flatpak Optimization)
        if (modelSize == "base") {
            // Standard Flatpak install path
            QString bundledPath = "/app/share/toice/assets/models/ggml-base.en.bin";
            
            // Also check local relative path for dev builds
            if (!QFile::exists(bundledPath)) {
                bundledPath = QCoreApplication::applicationDirPath() + "/../share/toice/assets/models/ggml-base.en.bin";
            }
            
            if (QFile::exists(bundledPath)) {
                DatabaseManager::instance().setSetting("model_path", bundledPath);
                statusLabel->setText("Using Bundled Model (Instant Setup)");
                progress->setValue(100);
                progress->show();
                QTimer::singleShot(500, this, &QDialog::accept);
                return;
            }
        }

        QString url = QString("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-%1.en.bin").arg(modelSize);
        
        // Use standard writable path for Flatpak/Linux
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData + "/models");
        QString dest = appData + "/models/ggml-" + modelSize + ".en.bin";
        
        QFile *file = new QFile(dest);
        if (!file->open(QIODevice::WriteOnly)) {
            statusLabel->setText("Error opening file for writing at: " + dest);
            return;
        }

        progress->show();
        btnStart->setEnabled(false);
        modelSelector->setEnabled(false);
        
        QNetworkRequest request(url);
        reply = manager.get(request);

        connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 received, qint64 total) {
            if (total > 0) progress->setValue((received * 100) / total);
        });

        connect(reply, &QNetworkReply::readyRead, this, [=]() {
            file->write(reply->readAll());
        });

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() == QNetworkReply::NoError) {
                file->close();
                DatabaseManager::instance().setSetting("model_path", dest);
                statusLabel->setText("Setup Complete!");
                QTimer::singleShot(1000, this, &QDialog::accept);
            } else {
                statusLabel->setText("Download Error: " + reply->errorString());
                btnStart->setEnabled(true);
            }
            file->deleteLater();
            reply->deleteLater();
        });
    }

    QComboBox *modelSelector;
    QProgressBar *progress;
    QPushButton *btnStart;
    QLabel *statusLabel;
    QNetworkAccessManager manager;
    QNetworkReply *reply;
};

#endif
