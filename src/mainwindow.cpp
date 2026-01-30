#include "mainwindow.h"
#include "databasemanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QProcess>
#include <QScreen>
#include <QGuiApplication>
#include <QScrollBar>
#include <QDebug>
#include <QClipboard>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QColor>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qDebug() << "=== VERSION: Toice V1 ===";
    QString platform = QGuiApplication::platformName();
    qDebug() << "=== Display Platform:" << platform << "===";
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE", "unknown");
    qDebug() << "=== Session Type:" << sessionType << "===";
    
    if (platform == "wayland") {
        qDebug() << "WARNING: Wayland detected - absolute positioning may be restricted.";
    }

    // Set Window Icon explicitly from resource
    setWindowIcon(QIcon(":/assets/logo.svg"));

    // 0. Initialize Database
    DatabaseManager::instance().init();
    
    // 0. Initialize Global Shortcut EARLY (Super + Z)
    m_shortcut = new GlobalShortcut(this);
    connect(m_shortcut, &GlobalShortcut::keyPressed, this, [=]() {
        if (!isRecording && !m_isFinalizing) {
            toggleRecording(true); // ENABLE OVERLAY for Shortcut
            showOverlay();     
            overlay->raise();
        }
    });
    connect(m_shortcut, &GlobalShortcut::keyReleased, this, [=]() {
        if (isRecording) {
            toggleRecording();
        }
    });
    m_shortcut->registerShortcut();
    
    setupUi();
    setupTray();

    // 0.1 Load History from DB
    // 0.1 Load History from DB (Rich Format)
    auto history = DatabaseManager::instance().getHistory(100);
    // Reverse for display since we append to layout? 
    // DB returns "ORDER BY timestamp ASC" (Oldest First)
    // We want Newest at Top of UI list usually?
    // addHistoryItem default is append (bottom).
    // If we want newest at top, we should use prepend=true.
    // Wait, original code looped through history and called addHistoryItem(..., prepend=false/default).
    // But original comment said "Newest First from DB". 
    // Let's check DB: "ORDER BY timestamp ASC" -> Oldest, Newer, Newest.
    // If we iterate and append, we get: Top=Oldest ... Bottom=Newest.
    // User likely wants Newest at Bottom like a chat log? Or Top?
    // "Historical Logs" usually implies chat history, Newest at Bottom.
    // The loop iterates ASC. So bottom is newest.
    // Let's keep logic same.
    for (const auto &item : history) {
        // Append to list (Oldest -> Newest)
        addHistoryItem(item.first, item.second);
    }
    
    // Scroll to bottom initially?
    QTimer::singleShot(100, [=]() {
        historyScrollArea->verticalScrollBar()->setValue(historyScrollArea->verticalScrollBar()->maximum());
    });
    
    // Initialize Inference Worker
    inference = new InferenceWorker(this);
    // [REMOVED LIVE UPDATES CONNECTION]
    inference->start();
    
    // 1. Create Overlay FIRST
    overlay = new OverlayWidget(nullptr);
    if (!overlay) {
        qCritical() << "CRITICAL: Failed to create overlay widget!";
    } else {
        qDebug() << "Overlay created successfully at" << overlay;
        // Connect stop signal to toggleRecording instead of showing main window
        // This ensures the app is finalized properly while overlay is visible.
        connect(overlay, &OverlayWidget::stopOverlay, this, [=]() {
            if (isRecording) toggleRecording();
        });
    }
    
    // 2. Create Audio SECOND
    audio = new AudioRecorder(this);
    if (audio) {
        connect(audio, &AudioRecorder::audioLevel, this, &MainWindow::updateAudioLevel);
        
        // 3. Connect to overlay only if it exists
        if (overlay) {
            connect(audio, &AudioRecorder::audioLevel, overlay, &OverlayWidget::setAudioLevel);
            connect(audio, &AudioRecorder::frequencyBands, overlay, &OverlayWidget::setFrequencyBands);
            qDebug() << "Connected levels to overlay";
        }

        // 4. Final Result Handling
        connect(inference, &InferenceWorker::finalResultReady, this, [=](QString text) {
            // If overlay is visible, give it a small delay to ensure Finalizing animation renders
            // User requested 1s minimum delay
            qint64 elapsed = m_finalizationStartTime.elapsed();
            int remaining = qMax(0, 1000 - (int)elapsed);
            
            qDebug() << "finalResultReady: elapsed =" << elapsed << "ms, overlay visible =" << overlay->isVisible() << ", delay =" << remaining;
            
            auto finalizeAction = [=]() {
                if (!text.isEmpty()) {
                    QGuiApplication::clipboard()->setText(text);
                    qDebug() << "Final transcription synced to clipboard:" << text;
                    
                    // PERSIST TO DATABASE
                    DatabaseManager::instance().addHistory(text);
                    
                    // Add to UI (Rich Format, Append/Bottom)
                    QString timeStr = QDateTime::currentDateTime().toString("hh:mm AP");
                    addHistoryItem(text, timeStr, false);
                    // Scroll to Bottom
                    QTimer::singleShot(50, [=]() {
                         historyScrollArea->verticalScrollBar()->setValue(historyScrollArea->verticalScrollBar()->maximum());
                    });
                    
                    if (overlay->isVisible()) {
                        qDebug() << "Calling showSuccessMessage after Finalizing delay";
                        overlay->showSuccessMessage("✓ Copied to Clipboard");
                        // Overlay will hide itself after success message duration
                    }
                } else {
                    if (overlay->isVisible()) overlay->hide();
                }
                
                btnRecord->setText("⏺ Start Recording");
                btnRecord->setStyleSheet("background: #22c55e; color: white; padding: 8px 16px; border-radius: 4px; border:none;");
                m_isFinalizing = false;
                
                // Update label with final text (Persistence)
                if (!text.isEmpty()) {
                    liveLabel->setText(text);
                    liveLabel->setStyleSheet("font-size: 24px; color: #000000; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
                    // HIDE META DATA (Clean View)
                    micIcon->hide();
                    subLabel->hide();
                    // SHOW ACTIONS
                    btnCopy->show();
                    btnClear->show();
                } else {
                    liveLabel->setText("Waiting for audio...");
                    liveLabel->setStyleSheet("font-size: 24px; color: #18181b; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
                    micIcon->show();
                    subLabel->show();
                    // HIDE ACTIONS
                    btnCopy->hide();
                    btnClear->hide();
                }
            };

            if (remaining > 0) {
                QTimer::singleShot(remaining, this, finalizeAction);
            } else {
                finalizeAction();
            }
        });
    }



    // 6. DBus Registration for Global Control
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.registerService("com.toice.app")) {
        bus.registerObject("/", this, QDBusConnection::ExportAllSlots);
        qDebug() << "DBus service registered: com.toice.app";
    }
}

void MainWindow::toggleFromRemote()
{
    qDebug() << "DBus: Remote toggle requested";
    toggleTranscription();
}

void MainWindow::toggleTranscription()
{
    if (!m_isFinalizing) {
        toggleRecording(true); // Always use overlay when toggling remotely or via shortcut
        if (isRecording) {
            showOverlay();
            overlay->raise();
        }
    }
}

void MainWindow::startFromRemote()
{
    qDebug() << "DBus: Remote START requested";
    if (!isRecording && !m_isFinalizing) {
        toggleRecording(true); // ENABLE OVERLAY for Remote
        showOverlay();
        overlay->raise();
    }
}

void MainWindow::stopFromRemote()
{
    qDebug() << "DBus: Remote STOP requested";
    if (isRecording) {
        toggleRecording(); // Sets isRecording to false
    }
}

MainWindow::~MainWindow() {}

void MainWindow::onDeviceChanged(int index)
{
    if (index >= 0 && index < devices.size()) {
        audio->setDevice(devices[index]);
        if (isRecording) audio->start();
    }
}

// [Modified toggleRecording signature]
void MainWindow::toggleRecording(bool useOverlay)
{
    if (m_isFinalizing) return;

    if (isRecording) {
        // STOPPING
        m_finalizationStartTime.start(); // Start timing the loading phase
        audio->stop();
        isRecording = false;
        m_isFinalizing = true;
        
        btnRecord->setText("Processing...");
        btnRecord->setStyleSheet("background: #27272a; color: #a1a1aa; padding: 8px 16px; border-radius: 4px; border:none;");
        
        // [1] COOL ANIMATION: Transition Pill -> Rotating Circle
        // Show animation if overlay is currently visible
        qDebug() << "=== STOP RECORDING === overlay visible:" << overlay->isVisible();
        if (overlay->isVisible()) {
            qDebug() << "Calling overlay->showSuccessState()";
            overlay->showSuccessState();
        }
        // Note: Don't forcibly hide overlay here - let it complete its animation
        // The overlay will hide itself after success message animation (see overlaywidget.cpp)
        
        // [2] TRIGGER SINGLE-PASS TRANSCRIPTION
        QVector<float> fullRecordedBuffer = audio->getRecordedAudio();
        qDebug() << "Captured full buffer for transcription:" << fullRecordedBuffer.size() << "samples";
        inference->requestFinalTranscription(fullRecordedBuffer);
        
        // [3] Log to History (Rich Format)
        // Get Formatted Time for UI
        QString timeStr = QDateTime::currentDateTime().toString("hh:mm AP");
        // We will add the item once the transcription is ready? 
        // Logic check: toggleRecording stops audio, then inference runs. 
        // Inference result comes back asynchronously in `updateTranscription`.
        // We should add to history THERE, not here.
        // Wait, existing code added to history where?
        // It was adding in `updateTranscription`? Let's check.
        // I don't see historyList->addItem in toggleRecording in my previous read.
        // It must be in `updateTranscription` or strictly manual?
        // Let's check `updateTranscription` quickly or assume I should add it here as a placeholder?
        // No, `inference->requestFinalTranscription` triggers `finalResultReady`.
        // `finalResultReady` lambda calls `updateTranscription`?
        // Let's look at `inference` connection locally (Lines ~94).
        // It calls `finalizeAction` -> `updateTranscription`.
        // Let's check `updateTranscription`.
        
        // Actually, for now, let's just make sure we capture the time here if needed, 
        // But correct place is likely `updateTranscription` or the finalizer lambda.
        // Let's UNTOUCH toggleRecording for now and check the lambda.

        
    } else {
        // STARTING
        m_usingOverlay = useOverlay; // Store state for this session

        inference->clear();
        liveLabel->setText("Listening...");
        audio->start();
        btnRecord->setText("⏹ Stop Recording");
        btnRecord->setStyleSheet("background: #ef4444; color: white; padding: 8px 16px; border-radius: 4px; border:none;");
        isRecording = true;
        
        // Reset overlay to recording pill ONLY if requested
        qDebug() << "[START RECORDING] m_usingOverlay =" << m_usingOverlay;
        if (m_usingOverlay) {
            qDebug() << "Calling overlay->updateStatus(true)";
            overlay->updateStatus(true);
            qDebug() << "After updateStatus, overlay visible =" << overlay->isVisible();
        } else {
            qDebug() << "NOT using overlay mode, hiding overlay";
            if (overlay->isVisible()) overlay->hide();
        }
        
        // Ensure UI is in recording state (Mic Visible)
        micIcon->show();
        subLabel->show();
        liveLabel->setStyleSheet("font-size: 24px; color: #18181b; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
    }
}

void MainWindow::showMainWindow()
{
    // Stop recording when returning from overlay
    if (isRecording) {
        toggleRecording();
    }
    overlay->hide();
    show();
}

void MainWindow::typeText()
{
    QString text = liveLabel->text();
    // Basic cleanup of placeholder
    if (text.contains("Waiting for audio")) return;
    
    // Use xdotool to type
    // We start detached because we don't want to block
    QProcess::startDetached("xdotool", QStringList() << "type" << "--delay" << "10" << text);
}

void MainWindow::updateAudioLevel(float level)
{
    // Level is 0.0 to 1.0 (mostly small values)
    int val = static_cast<int>(level * 1000); // Scale up
    if (val > 100) val = 100;
    audioMeter->setValue(val);
    
    // Pass raw level to overlay for visualization
    if (overlay && overlay->isVisible()) {
        overlay->setAudioLevel(level); // Pass raw 0.0-1.0 level
    }
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    central->setStyleSheet("background-color: #e4e4e7; font-family: 'Inter', sans-serif;"); // Zinc-200 for better contrast
    setCentralWidget(central);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // --- 1. HEADER (Top Bar) ---
    // --- 1. HEADER (Top Bar) ---
    QWidget *header = new QWidget();
    header->setFixedHeight(60);
    header->setStyleSheet("background: rgba(255,255,255,0.95); border-bottom: 1px solid #f4f4f5;");
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 0, 24, 0);

    // --- ICON DRAWING HELPERS ---
    // Logo Icon
    QLabel *logoIcon = new QLabel();
    logoIcon->setFixedSize(24, 24);
    
    // Use the user-provided vector asset directly
    QPixmap logoPix = QIcon(":/assets/header_logo.svg").pixmap(24, 24);
    
    logoIcon->setPixmap(logoPix);
    headerLayout->addWidget(logoIcon);
    
    // Logo Text
    QLabel *logo = new QLabel("Toice");
    logo->setStyleSheet("font-family: 'Inter', sans-serif; font-size: 18px; font-weight: 600; color: #18181b; margin-left: 8px;");
    headerLayout->addWidget(logo);
    
    headerLayout->addStretch();
    
    // Engine Ready Chip
    QWidget *statusChip = new QWidget();
    statusChip->setStyleSheet("background: #f4f4f5; border-radius: 14px;"); 
    statusChip->setFixedHeight(28); 
    QHBoxLayout *chipLayout = new QHBoxLayout(statusChip);
    chipLayout->setContentsMargins(10, 0, 10, 0); 
    chipLayout->setSpacing(6);
    
    QWidget *dot = new QWidget();
    dot->setFixedSize(6, 6);
    dot->setStyleSheet("background: #22c55e; border-radius: 3px;"); 
    chipLayout->addWidget(dot);
    
    QLabel *statusText = new QLabel("Engine Ready");
    statusText->setStyleSheet("font-family: 'Inter', sans-serif; font-size: 12px; font-weight: 500; color: #52525b;");
    chipLayout->addWidget(statusText);
    headerLayout->addWidget(statusChip);
    
    mainLayout->addWidget(header);

    // --- 2. MAIN CONTENT AREA ---
    QWidget *content = new QWidget();
    QHBoxLayout *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->setSpacing(24);
    
    // === LEFT: TRANSCRIPTION CARD ===
    QWidget *leftPanel = new QWidget();
    leftPanel->setObjectName("leftPanel");
    // Card style: White bg, Border, Rounded
    leftPanel->setStyleSheet(
        "QWidget#leftPanel { "
        "  background: white; "
        "  border: 1px solid #e4e4e7; "
        "  border-radius: 16px; " 
        "}"
    );
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    
    // Card Header (Transparent for continuity)
    QWidget *paneHeader = new QWidget();
    paneHeader->setFixedHeight(50);
    // FORCE TRANSPARENCY AND REMOVE BORDER AGGRESSIVELY
    paneHeader->setStyleSheet("background-color: transparent; border: 0px; margin: 0px; padding: 0px;");
    QHBoxLayout *phLayout = new QHBoxLayout(paneHeader);
    phLayout->setContentsMargins(24, 0, 24, 0);
    
    QLabel *phTitle = new QLabel("TRANSCRIPTION ENGINE");
    phTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #71717a; letter-spacing: 0.5px;");
    phLayout->addWidget(phTitle);
    phLayout->addStretch();
    
    // Draw Mic Helper
    auto drawMic = [](QPainter& p) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#e4e4e7")); // Light gray bg for capsule
        p.setPen(Qt::NoPen);
        
        // Mic Capsule
        p.drawRoundedRect(22, 12, 20, 28, 10, 10);
        
        // Mic Grille pattern (dots)
        p.setBrush(QColor("#a1a1aa"));
        p.drawEllipse(26, 18, 2, 2); p.drawEllipse(32, 18, 2, 2); p.drawEllipse(38, 18, 2, 2);
        p.drawEllipse(29, 24, 2, 2); p.drawEllipse(35, 24, 2, 2);
        p.drawEllipse(26, 30, 2, 2); p.drawEllipse(32, 30, 2, 2); p.drawEllipse(38, 30, 2, 2);

        // Stand/Holder
        QPen standPen(QColor("#a1a1aa"), 3, Qt::SolidLine, Qt::RoundCap);
        p.setPen(standPen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(14, 24, 36, 26, 180 * 16, 180 * 16); // U-shape holder
        
        p.drawLine(32, 50, 32, 56); // Stem
        p.drawLine(24, 56, 40, 56); // Base
    };

    // Action Buttons
    QWidget *actionsHost = new QWidget();
    QHBoxLayout *actLayout = new QHBoxLayout(actionsHost);
    actLayout->setContentsMargins(0, 0, 0, 0);
    actLayout->setSpacing(12);
    
    btnCopy = new QPushButton("Copy");
    btnCopy->setCursor(Qt::PointingHandCursor);
    btnCopy->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 11px; font-weight: 600; color: #71717a; } QPushButton:hover { color: #18181b; }");
    btnCopy->hide(); // Hide initially
    connect(btnCopy, &QPushButton::clicked, this, [=](){ 
        QGuiApplication::clipboard()->setText(liveLabel->text()); 
        btnCopy->setText("Copied!");
        QTimer::singleShot(1500, btnCopy, [=](){ btnCopy->setText("Copy"); });
    });
    
    btnClear = new QPushButton("Clear");
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 11px; font-weight: 600; color: #71717a; } QPushButton:hover { color: #ef4444; }");
    btnClear->hide(); // Hide initially
    connect(btnClear, &QPushButton::clicked, this, [=](){ 
        liveLabel->setText("Waiting for audio...");
        liveLabel->setStyleSheet("font-size: 24px; color: #18181b; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
        micIcon->show();
        subLabel->show();
        btnCopy->hide(); // Hide actions on clear
        btnClear->hide();
    });

    actLayout->addWidget(btnCopy);
    actLayout->addWidget(btnClear);
    phLayout->addWidget(actionsHost);
    
    leftLayout->addWidget(paneHeader);

    // Card Body: Empty State
    QWidget *body = new QWidget();
    // EXPLICITLY TRANSPARENT TO SHOW PARENT WHITE BG
    body->setStyleSheet("background: transparent;");
    QVBoxLayout *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setAlignment(Qt::AlignCenter);
    
    // Mic Visual
    micIcon = new QLabel();
    micIcon->setFixedSize(64, 64);
    QPixmap micPix(64, 64);
    micPix.fill(Qt::transparent);
    {
        QPainter p(&micPix);
        drawMic(p);
    }
    micIcon->setPixmap(micPix);
    micIcon->setStyleSheet("margin-bottom: 16px;");
    bodyLayout->addWidget(micIcon, 0, Qt::AlignCenter);
    
    // Metadata Label (New)
    msgTimeLabel = new QLabel(""); 
    msgTimeLabel->setStyleSheet("font-size: 12px; color: #a1a1aa; font-weight: 600; text-transform: uppercase; margin-bottom: 4px;");
    msgTimeLabel->setAlignment(Qt::AlignLeft); // Align with text? Or Center?
    // User image shows text left aligned in card? Or centered?
    // Current liveLabel is centered. Let's Center this too for consistency locally.
    msgTimeLabel->setAlignment(Qt::AlignCenter); 
    msgTimeLabel->hide(); // Hidden initially
    bodyLayout->addWidget(msgTimeLabel);

    liveLabel = new QLabel("Waiting for audio...");
    liveLabel->setAlignment(Qt::AlignCenter);
    liveLabel->setWordWrap(true); // Fix wrapping
    liveLabel->setStyleSheet("font-size: 24px; color: #18181b; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
    bodyLayout->addWidget(liveLabel);
    
    subLabel = new QLabel("Start speaking or play an audio file. The transcription will appear here in real-time.");
    subLabel->setStyleSheet("font-size: 14px; color: #71717a;");
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setWordWrap(true);
    bodyLayout->addWidget(subLabel);
    
    leftLayout->addWidget(body, 1);
    
    // Card Footer (Controls - Transparent)
    QWidget *bottomBar = new QWidget();
    bottomBar->setFixedHeight(80); // Slightly taller for breathing room
    // FORCE TRANSPARENCY AND REMOVE BORDER AGGRESSIVELY
    bottomBar->setStyleSheet("background-color: transparent; border: 0px; margin: 0px; padding: 0px;");
    QHBoxLayout *controls = new QHBoxLayout(bottomBar);
    controls->setContentsMargins(20, 0, 20, 0);
    controls->setSpacing(16);
    
    // Device Selector Box
    QWidget *micBox = new QWidget();
    micBox->setFixedSize(220, 36); // Fixed dimensions
    micBox->setStyleSheet(
        "QWidget { background: white; border: 1px solid #e4e4e7; border-radius: 6px; }"
        "QWidget:hover { border: 1px solid #a1a1aa; }"
    );
    QHBoxLayout *micLayout = new QHBoxLayout(micBox);
    micLayout->setContentsMargins(10, 0, 10, 0);
    micLayout->setSpacing(8);
    
    // 1. Mic Icon (Simple & Bold)
    QLabel *lblDeviceIcon = new QLabel();
    lblDeviceIcon->setFixedSize(14, 14);
    QPixmap miniMic(14, 14);
    miniMic.fill(Qt::transparent);
    {
        QPainter p(&miniMic);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#52525b")); // Zinc-600
        p.setPen(Qt::NoPen);
        // Simple Capsule
        p.drawRoundedRect(4, 0, 6, 10, 3, 3);
        // Base Line
        p.drawRect(6, 11, 2, 2); // Neck
        p.drawRoundedRect(3, 13, 8, 1, 0.5, 0.5); // Base
    }
    lblDeviceIcon->setPixmap(miniMic);
    lblDeviceIcon->setStyleSheet("border: none;");
    micLayout->addWidget(lblDeviceIcon);
    
    // 2. ComboBox (Flexible Width)
    deviceSelector = new QComboBox();
    deviceSelector->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); // Allow shrinking
    deviceSelector->setCursor(Qt::PointingHandCursor);
    deviceSelector->setStyleSheet(
        "QComboBox { border: none; background: transparent; font-size: 13px; font-weight: 500; color: #18181b; padding: 0px; }"
        "QComboBox::drop-down { border: none; width: 0px; }"
        "QComboBox QAbstractItemView { "
        "  background: white; "
        "  border: 1px solid #e4e4e7; "
        "  selection-background-color: #f4f4f5; "
        "  selection-color: #18181b; "
        "  color: #18181b; "
        "  outline: none; "
        "  padding: 4px; "
        "}"
    );
    micLayout->addWidget(deviceSelector, 1); // Expand to fill
    
    // 3. Arrow Icon (Right Aligned)
    QLabel *arrowIcon = new QLabel();
    arrowIcon->setFixedSize(10, 10);
    QPixmap arrowPix(10, 10);
    arrowPix.fill(Qt::transparent);
    {
        QPainter p(&arrowPix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#52525b"), 2, Qt::SolidLine, Qt::RoundCap));
        // Chevron
        QPainterPath path;
        path.moveTo(1, 3);
        path.lineTo(5, 7);
        path.lineTo(9, 3);
        p.drawPath(path);
    }
    arrowIcon->setPixmap(arrowPix);
    arrowIcon->setStyleSheet("border: none;");
    micLayout->addWidget(arrowIcon);
    
    devices = AudioRecorder::availableDevices();
    for (const auto &dev : devices) deviceSelector->addItem(dev.description());
    connect(deviceSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);
    micLayout->addWidget(deviceSelector);
    
    controls->addWidget(micBox);
    
    // Visualizer Bars
    audioMeter = new QProgressBar();
    audioMeter->setRange(0, 100);
    audioMeter->setFixedWidth(60);
    audioMeter->setTextVisible(false);
    audioMeter->setStyleSheet(
        "QProgressBar { border: none; background: #e4e4e7; height: 4px; border-radius: 2px; } "
        "QProgressBar::chunk { background: #18181b; border-radius: 2px; }"
    );
    controls->addWidget(audioMeter);
    
    controls->addStretch();
    
    // Start Button (Primary)
    btnRecord = new QPushButton("Start Recording");
    btnRecord->setStyleSheet(
        "QPushButton { background: #18181b; color: white; border-radius: 8px; padding: 10px 20px; font-weight: 600; font-size: 13px; }"
        "QPushButton:hover { background: #27272a; }"
    );
    connect(btnRecord, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    controls->addWidget(btnRecord);
    
    leftLayout->addWidget(bottomBar);
    contentLayout->addWidget(leftPanel, 1); // Expand left panel
    
    // === RIGHT: HISTORY SIDEBAR ===
    QWidget *rightPanel = new QWidget();
    rightPanel->setFixedWidth(300);
    rightPanel->setObjectName("rightPanel");
    rightPanel->setStyleSheet(
        "QWidget#rightPanel { "
        "  background: white; "
        "  border: 1px solid #e4e4e7; "
        "  border-radius: 16px; "
        "}"
    );
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    
    // Sidebar Header
    QWidget *rh = new QWidget();
    rh->setFixedHeight(50);
    rh->setStyleSheet("background: #fafafa; border-bottom: 1px solid #e4e4e7; border-top-left-radius: 16px; border-top-right-radius: 16px;");
    QHBoxLayout *rhLayout = new QHBoxLayout(rh);
    rhLayout->setContentsMargins(20, 0, 20, 0);
    QLabel *rhTitle = new QLabel("HISTORICAL LOGS");
    rhTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #71717a;");
    rhLayout->addWidget(rhTitle);
    rhLayout->addStretch();
    rightLayout->addWidget(rh);
    
    // List - REPLACED WITH QScrollArea
    // 1. Create Scroll Area
    historyScrollArea = new QScrollArea();
    historyScrollArea->setWidgetResizable(true); // CRITICAL: Allows container to resize with area
    historyScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // No HScroll
    historyScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    historyScrollArea->setFrameStyle(QFrame::NoFrame); // Clean look
    historyScrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { border: none; background: #fafafa; width: 6px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #d4d4d8; min-height: 20px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: #a1a1aa; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    
    // 2. Create Container Widget
    historyContainer = new QWidget();
    historyContainer->setObjectName("historyContainer");
    historyContainer->setStyleSheet("QWidget#historyContainer { background: transparent; }");
    
    // 3. Create Vertical Layout
    historyLayout = new QVBoxLayout(historyContainer);
    historyLayout->setContentsMargins(10, 10, 10, 10); // Padding around items
    historyLayout->setSpacing(8); // Gap between items
    historyLayout->setAlignment(Qt::AlignTop); // Stack items at top
    
    // 4. Set Container to Scroll Area
    historyScrollArea->setWidget(historyContainer);
    
    rightLayout->addWidget(historyScrollArea);
    
    // Sidebar Footer (Paginationish)
    QWidget *rf = new QWidget();
    rf->setFixedHeight(40);
    rf->setStyleSheet("background: #fafafa; border-top: 1px solid #e4e4e7; border-bottom-left-radius: 16px; border-bottom-right-radius: 16px;");
    QHBoxLayout *rfLayout = new QHBoxLayout(rf);
    rfLayout->setContentsMargins(16, 0, 16, 0);
    QLabel *pg = new QLabel("Viewing latest");
    pg->setStyleSheet("font-size: 10px; color: #a1a1aa;");
    rfLayout->addWidget(pg);
    rightLayout->addWidget(rf);
    
    contentLayout->addWidget(rightPanel);
    mainLayout->addWidget(content, 1);
    
    // --- 3. GLOBAL FOOTER ---
    QWidget *footer = new QWidget();
    footer->setFixedHeight(24);
    footer->setStyleSheet("background: white; border-top: 1px solid #e4e4e7;");
    QHBoxLayout *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 0, 24, 0);
    QLabel *vLabel = new QLabel("v2.4.0 • Connected to Local Whisper Service");
    vLabel->setStyleSheet("font-size: 10px; color: #a1a1aa;");
    footerLayout->addWidget(vLabel);
    footerLayout->addStretch();
    QLabel *readyLabel = new QLabel("Ready");
    readyLabel->setStyleSheet("font-size: 10px; color: #a1a1aa;");
    footerLayout->addWidget(readyLabel);
    
    mainLayout->addWidget(footer);
    
    resize(1100, 700);
}

void MainWindow::setupTray()
{
    // Create Tray Icon
    trayIcon = new QSystemTrayIcon(this);
    
    // CRITICAL FIX: Use QIcon::fromTheme for Flatpak SNI compatibility
    // Absolute paths inside sandbox don't work for StatusNotifierItem on host
    QIcon trayThemeIcon = QIcon::fromTheme("com.toice.app", QIcon(":/assets/icon_128.png"));
    trayIcon->setIcon(trayThemeIcon);
    
    trayIcon->setToolTip("Toice - Voice to Text");

    // Tray Menu
    QMenu *menu = new QMenu(this);
    QAction *showAction = menu->addAction("Settings");
    connect(showAction, &QAction::triggered, this, &MainWindow::showMainWindow);
    
    menu->addSeparator();
    
    QAction *quitAction = menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    
    trayIcon->setContextMenu(menu);
    
    // Toggle on click
    connect(trayIcon, &QSystemTrayIcon::activated, [=](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger) {
            if (isVisible()) {
                hide();
            } else {
                showMainWindow();
            }
        }
    });

    trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Instead of quitting, hide to tray
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::showOverlay()
{
    showMinimized(); // Keep in taskbar instead of fully hiding
    
    // Auto-start recording if not already recording
    if (!isRecording) {
        toggleRecording();
    }
    
    // Calculate position at bottom-center of screen
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen->availableGeometry(); // Always use availableGeometry to respect taskbars
    
    // Width and Height of the transparent overlay container (500x100)
    const int w = 500;
    const int h = 100;
    const int bottomMargin = 40; 
    
    int x = screenGeom.x() + (screenGeom.width() - w) / 2;
    int y = screenGeom.y() + screenGeom.height() - h - bottomMargin;
    
    qDebug() << "Overlay Container Position:" << x << "," << y << " (Screen:" << screenGeom << ")";
    
    // Aggressive Positioning
    overlay->setGeometry(x, y, w, h);
    overlay->show();
    overlay->raise();
    // overlay->activateWindow(); // REMOVED: prevents stealing focus from active input
    
    // Force move twice to ensure Wayland/KWin placement
    QTimer::singleShot(20, this, [=]() { overlay->move(x, y); });
    QTimer::singleShot(100, this, [=]() { overlay->move(x, y); });
    
    // Most Reliable: Use DBus to Bypass Window Manager (KDE specific)
    auto dbusMsg = QDBusMessage::createMethodCall(
        "org.kde.KWin",
        "/KWin",
        "org.kde.KWin",
        "setGeometry");
    dbusMsg << (int)overlay->winId() << x << y << w << h;
    QDBusConnection::sessionBus().call(dbusMsg);
    
    // Force position repeatedly for stubborn WMs
    QTimer::singleShot(100, [=]() {
        overlay->move(x, y);
        overlay->raise();
    });
    
    QTimer::singleShot(500, [=]() {
        // Final sanity check/force
        if (overlay->pos().y() < y - 50) { // If it's still way off (e.g. centered)
             overlay->move(x, y);
             qDebug() << "Position re-forced at 500ms. Current:" << overlay->pos();
        }
    });

    // Update status
    overlay->updateStatus(isRecording);
}

void MainWindow::addHistoryItem(const QString &text, const QString &time, bool prepend)
{
    // Create Custom Widget
    HistoryItemWidget *widget = new HistoryItemWidget(text, time);
    
    // Connect Click Signal to View Logic
    connect(widget, &HistoryItemWidget::clicked, this, [=](QString t, QString tm) {
        liveLabel->setText(t);
        liveLabel->setStyleSheet("font-size: 24px; color: #18181b; font-weight: 500; margin-bottom: 8px; padding: 0 20px;");
        
        msgTimeLabel->setText("Recorded at " + tm);
        msgTimeLabel->show();
        
        // Hide initial placeholders
        micIcon->hide();
        subLabel->hide();
        // Allow Copy/Clear for review mode too
        btnCopy->show();
        btnClear->show();
    });

    if (prepend) {
        historyLayout->insertWidget(0, widget);
    } else {
        historyLayout->addWidget(widget);
    }
    
    // Force layout update just in case
    historyContainer->updateGeometry();
}

void MainWindow::updateTranscription(QString text, bool isFinal)
{
    Q_UNUSED(isFinal); // we always show live updates on label
    if (text.isEmpty()) return;
    
    liveLabel->setText(text);
    // Compact pill overlay doesn't show text
    
    if (isFinal) {
        // If ever we want finalized entries in history, we'd add them here
        // For now, we manually add on Stop Recording
    }
}
