#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class SettingsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    QString getSelectedModelPath() const;
    int getSelectedShortcutPreset() const;

signals:
    void settingsSaved(QString modelPath, int presetIndex);

private:
    QComboBox *comboModel;
    QLabel *lblModelPath;
    QPushButton *btnBrowse;
    
    QComboBox *comboShortcut;
    
    QString m_customModelPath;
};

#endif // SETTINGSDIALOG_H
