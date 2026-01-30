#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include "overlaywidget.h"
#include "inferenceworker.h"
#include "audiorecorder.h"
#include <QComboBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QSystemTrayIcon>
#include <QPushButton>
#include <QElapsedTimer>
#include "globalshortcut.h"
#include <QCloseEvent>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QClipboard>
#include <QGuiApplication>

class HistoryItemWidget : public QWidget {
    Q_OBJECT
public:
    HistoryItemWidget(const QString &text, const QString &time, QWidget *parent = nullptr) 
        : QWidget(parent), m_text(text.trimmed()), m_time(time) {
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor); // Clickable indication
        setStyleSheet(
            "HistoryItemWidget { background: #ffffff; border: 1px solid #e4e4e7; border-radius: 8px; }"
            "HistoryItemWidget:hover { background: #fafafa; border: 1px solid #d4d4d8; }"
        );
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 6); // Reduced bottom padding (12->6)
        layout->setSpacing(4); // Reduced spacing
        layout->setSizeConstraint(QLayout::SetMinimumSize); 
        layout->setAlignment(Qt::AlignTop); // Enforce Top Alignment for Layout

        // Header: Time + Spacer + Copy Button
        QHBoxLayout *header = new QHBoxLayout();
        header->setContentsMargins(0, 0, 0, 0);
        
        QLabel *timeDesc = new QLabel(time);
        timeDesc->setStyleSheet("font-size: 10px; color: #a1a1aa; font-weight: 500; background: transparent; border: none;");
        header->addWidget(timeDesc);
        
        header->addStretch();
        
        btnCopy = new QPushButton();
        btnCopy->setFixedSize(20, 20);
        btnCopy->setCursor(Qt::PointingHandCursor);
        btnCopy->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 4px; } QPushButton:hover { background: #e4e4e7; }");
        QIcon copyIcon;
        QPixmap copyPix(16, 16);
        copyPix.fill(Qt::transparent);
        QPainter p(&copyPix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#71717a"), 1.5));
        p.drawRoundedRect(3, 3, 7, 7, 1, 1);
        p.drawLine(6, 2, 11, 2); p.drawLine(12, 3, 12, 8); // Offset rect hint
        copyIcon.addPixmap(copyPix);
        btnCopy->setIcon(copyIcon);
        btnCopy->hide();
        
        connect(btnCopy, &QPushButton::clicked, this, [=]() {
           QGuiApplication::clipboard()->setText(m_text);
        });
        
        header->addWidget(btnCopy);
        layout->addLayout(header);

        // Text Body
        m_bodyLabel = new QLabel(m_text);
        m_bodyLabel->setWordWrap(true);
        m_bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft); // Enforce Top Alignment for Text
        m_bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse); 
        m_bodyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        m_bodyLabel->setStyleSheet("font-size: 13px; color: #18181b; background: transparent; border: none; padding: 0px;");
        layout->addWidget(m_bodyLabel);
        
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // Horizontal Expand, Vertical Fixed
        setAttribute(Qt::WA_Hover);
    }
    
signals:
    void clicked(QString text, QString time);

protected:
    void mousePressEvent(QMouseEvent *event) override {
        // Only trigger if not clicking the copy button (though copy button is on top)
        // Simple heuristic: always emit.
        emit clicked(m_text, m_time);
        QWidget::mousePressEvent(event);
    }
    
    void enterEvent(QEnterEvent *event) override {
        btnCopy->show();
        update();
    }
    void leaveEvent(QEvent *event) override {
        btnCopy->hide();
        update();
    }

private:
    QPushButton *btnCopy;
    QLabel *m_bodyLabel;
    QString m_text;
    QString m_time;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.toice.app.Native")

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void toggleFromRemote();
    void startFromRemote();
    void stopFromRemote();
    void showOverlay();
    void updateTranscription(QString text, bool isFinal);
    void onDeviceChanged(int index);
    void updateAudioLevel(float level);
    void toggleTranscription();
    void toggleRecording(bool useOverlay = false);
    void showMainWindow();
    void typeText();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupTray();
    void addHistoryItem(const QString &text, const QString &time, bool prepend = false);
    
    OverlayWidget *overlay;
    // QListWidget *historyList; // REPLACED
    QScrollArea *historyScrollArea;
    QWidget *historyContainer;
    QVBoxLayout *historyLayout;
    
    InferenceWorker *inference;
    AudioRecorder *audio;
    QLabel *liveLabel;
    QLabel *msgTimeLabel; 
    QSystemTrayIcon *trayIcon;
    
    QComboBox *deviceSelector;
    QProgressBar *audioMeter;
    QPushButton *btnRecord;
    bool isRecording = false;
    bool m_isFinalizing = false;
    QList<QAudioDevice> devices;
    GlobalShortcut *m_shortcut;
    bool m_usingOverlay = false;



    QElapsedTimer m_finalizationStartTime;
    
    // UI Members for visibility toggling
    QLabel *micIcon;
    QLabel *subLabel;
    QPushButton *btnCopy;
    QPushButton *btnClear;
};

#endif // MAINWINDOW_H
