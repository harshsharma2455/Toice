#ifndef OVERLAY_H
#define OVERLAY_H
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    OverlayWidget(QWidget *parent = nullptr);
    enum OverlayState { Recording, Finalizing, Success };
    void updateStatus(bool isRecording);
    void setAudioLevel(float level);
    void setFrequencyBands(const QVector<float> &bands);
    void showSuccessState(); // Transition to rotating circle
    void showSuccessMessage(const QString &msg); // Transition to text message

signals:
    void stopOverlay();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Window dragging
    QPoint m_dragPosition;
    
    // Visualization state
    float currentLevel = 0.0f;
    float m_barHeights[6]; // Store heights for each bar
    float m_barPhases[6];  // Independent timing for each bar
    QVector<float> waveHistory;
    
    // Animation State Machine
    OverlayState m_state = Recording;
    bool m_isHovered = false;
    float m_hoverProgress = 0.0f;
    float m_pulseScale = 1.0f;
    bool m_pulseGrowing = true;
    float m_loaderRotation = 0.0f;
    float m_opacity = 1.0f;
    
    // Morphing Layout
    float m_currentWidth = 133.0f;
    float m_targetWidth = 133.0f;
    float m_pillHeight = 49.0f;
    
    QString m_message;
    QTimer *pulseTimer;

    // Finalizing state timing
    QElapsedTimer m_finalizingTimer;
};
#endif
