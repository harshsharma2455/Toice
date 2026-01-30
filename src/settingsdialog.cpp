#include "settingsdialog.h"
#include "databasemanager.h"
#include "globalshortcut.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QCoreApplication>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Settings");
    setFixedSize(500, 350);
    setStyleSheet("background: white; font-family: 'Inter', sans-serif;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(20);

    // --- 1. MODEL SELECTION ---
    QGroupBox *grpModel = new QGroupBox("AI Model");
    grpModel->setStyleSheet("QGroupBox { border: 1px solid #e4e4e7; border-radius: 8px; margin-top: 10px; font-weight: 600; color: #18181b; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *modelLayout = new QVBoxLayout(grpModel);
    
    comboModel = new QComboBox();
    comboModel->addItem("Default (Small English)", "default");
    comboModel->addItem("Custom Path...", "custom");
    comboModel->setStyleSheet("padding: 8px; border: 1px solid #d4d4d8; border-radius: 4px;");
    
    lblModelPath = new QLabel("Path: Built-in");
    lblModelPath->setStyleSheet("color: #71717a; font-size: 11px;");
    lblModelPath->setWordWrap(true);
    
    btnBrowse = new QPushButton("Browse...");
    btnBrowse->setCursor(Qt::PointingHandCursor);
    btnBrowse->setEnabled(false); // Default disabled
    
    modelLayout->addWidget(comboModel);
    modelLayout->addWidget(btnBrowse);
    modelLayout->addWidget(lblModelPath);
    
    mainLayout->addWidget(grpModel);

    // --- 2. SHORTCUT SELECTION ---
    QGroupBox *grpShortcut = new QGroupBox("Global Shortcut");
    grpShortcut->setStyleSheet("QGroupBox { border: 1px solid #e4e4e7; border-radius: 8px; margin-top: 10px; font-weight: 600; color: #18181b; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *shortLayout = new QVBoxLayout(grpShortcut);
    
    comboShortcut = new QComboBox();
    comboShortcut->addItem("Super + Z (Windows + Z)", GlobalShortcut::SuperZ);
    comboShortcut->addItem("Ctrl + Space", GlobalShortcut::CtrlSpace);
    comboShortcut->addItem("Super + Space", GlobalShortcut::SuperSpace);
    comboShortcut->addItem("Ctrl + Alt + Z", GlobalShortcut::CtrlAltZ);
    comboShortcut->setStyleSheet("padding: 8px; border: 1px solid #d4d4d8; border-radius: 4px;");
    
    shortLayout->addWidget(comboShortcut);
    mainLayout->addWidget(grpShortcut);
    
    mainLayout->addStretch();

    // --- 3. BUTTONS ---
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton *btnCancel = new QPushButton("Cancel");
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setStyleSheet("background: #f4f4f5; color: #18181b; border: none; padding: 8px 16px; border-radius: 6px;");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    
    QPushButton *btnSave = new QPushButton("Save Changes");
    btnSave->setCursor(Qt::PointingHandCursor);
    btnSave->setStyleSheet("background: #18181b; color: white; border: none; padding: 8px 16px; border-radius: 6px;");
    connect(btnSave, &QPushButton::clicked, this, [=]() {
        QString finalPath = (comboModel->currentData().toString() == "default") 
                            ? QCoreApplication::applicationDirPath() + "/models/ggml-base.en.bin"
                            : m_customModelPath;
        
        emit settingsSaved(finalPath, comboShortcut->currentData().toInt());
        accept();
    });
    
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnSave);
    mainLayout->addLayout(btnLayout);
    
    // --- LOGIC WIRING ---
    connect(comboModel, &QComboBox::currentIndexChanged, this, [=](int index) {
        bool isCustom = (comboModel->itemData(index).toString() == "custom");
        btnBrowse->setEnabled(isCustom);
        if (!isCustom) {
            lblModelPath->setText("Path: Built-in Default");
        } else {
            lblModelPath->setText(m_customModelPath.isEmpty() ? "Path: None selected" : m_customModelPath);
        }
    });
    
    connect(btnBrowse, &QPushButton::clicked, this, [=]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Model File", "", "Model Files (*.bin)");
        if (!path.isEmpty()) {
            m_customModelPath = path;
            lblModelPath->setText(path);
        }
    });
    
    // --- LOAD CURRENT SETTINGS ---
    QString currentModel = DatabaseManager::instance().getSetting("model_path");
    QString defaultModel = QCoreApplication::applicationDirPath() + "/models/ggml-base.en.bin";
    
    if (currentModel.isEmpty() || currentModel == defaultModel) {
        comboModel->setCurrentIndex(0);
    } else {
        comboModel->setCurrentIndex(1);
        m_customModelPath = currentModel;
        lblModelPath->setText(currentModel);
    }
    
    int currentPreset = DatabaseManager::instance().getSetting("shortcut_preset", "0").toInt(); // 0 = SuperZ
    int idx = comboShortcut->findData(currentPreset);
    if (idx >= 0) comboShortcut->setCurrentIndex(idx);
}
