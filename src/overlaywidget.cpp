#include "overlaywidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>
#include <QMoveEvent>

OverlayWidget::OverlayWidget(QWidget *parent) : QWidget(parent) {
    // RESTORED: Frameless and Transparent
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    m_currentWidth = 133.0f;
    m_targetWidth = 133.0f;
    // Set a large enough fixed size for the transparent container to avoid resizing jitter
    setFixedSize(500, 100); 

    for(int i=0; i<6; ++i) {
        m_barHeights[i] = 4.0f;
        m_barPhases[i] = (float)i * 1.5f;
    }
    
    pulseTimer = new QTimer(this);
    connect(pulseTimer, &QTimer::timeout, this, [=]() {
        if (m_state == Recording) {
            // Pulse logic for red hover circle
            if (m_pulseGrowing) {
                m_pulseScale += 0.015f; // restore 60fps speed
                if (m_pulseScale >= 1.0f) m_pulseGrowing = false;
            } else {
                m_pulseScale -= 0.015f;
                if (m_pulseScale <= 0.6f) m_pulseGrowing = true;
            }
        } else if (m_state == Finalizing) {
            m_loaderRotation += 6.0f;
            if (m_loaderRotation >= 360.0f) m_loaderRotation -= 360.0f;
        } else if (m_state == Success) {
            m_opacity -= 0.01f;
            if (m_opacity <= 0.0f) {
                hide();
                pulseTimer->stop();
            }
        }
        
        // Handle Hover Animation Transition
        if (m_isHovered) {
            m_hoverProgress = qMin(1.0f, m_hoverProgress + 0.15f);
        } else {
            m_hoverProgress = qMax(0.0f, m_hoverProgress - 0.15f);
        }

        // Handle Width Morphing with dynamic speed
        float speed = 0.2f;
        if (m_state == Finalizing) speed = 0.3f; // Faster shrink
        if (m_state == Success) speed = 0.4f;    // Snappy expansion
        
        m_currentWidth = m_currentWidth * (1.0f - speed) + m_targetWidth * speed;
        if (std::abs(m_currentWidth - m_targetWidth) < 0.1f) m_currentWidth = m_targetWidth;
        
        update();
    });
}

void OverlayWidget::updateStatus(bool isRecording) {
    if (isRecording) {
        m_state = Recording;
        m_targetWidth = 133.0f;
        m_opacity = 1.0f;
        m_message = "";
        show();
        raise();
        if (!pulseTimer->isActive()) pulseTimer->start(16);
    }
    update();
}

void OverlayWidget::showSuccessState() {
    qDebug() << "OverlayWidget::showSuccessState() - Starting Finalizing animation";
    m_state = Finalizing;
    m_targetWidth = 50.0f; // Morph to circle (was 80.0f, user wants circle)
    m_opacity = 1.0f;
    m_loaderRotation = 0.0f; // Reset rotation
    m_finalizingTimer.start(); // Start timing the Finalizing state
    show();
    raise();
    if (!pulseTimer->isActive()) {
        qDebug() << "Starting pulseTimer for Finalizing state";
        pulseTimer->start(16);
    }
    update();
}

void OverlayWidget::showSuccessMessage(const QString &msg) {
    qDebug() << "OverlayWidget::showSuccessMessage() called, current state:" << m_state;
    
    // Define transition logic
    auto doTransition = [this, msg]() {
        qDebug() << "Transitioning to Success state with message:" << msg;
        m_state = Success;
        m_message = msg;
        QFont font;
        font.setBold(true);
        font.setPixelSize(12);
        QFontMetrics metrics(font);
        int textW = metrics.horizontalAdvance(msg);
        m_targetWidth = qMax(133.0f, 60.0f + textW + 20.0f);
        m_opacity = 1.0f;
        if (!pulseTimer->isActive()) pulseTimer->start(16);
        update();
    };

    // If we're in Finalizing state, delay before transitioning to Success
    if (m_state == Finalizing) {
        qDebug() << "In Finalizing state - applying 1.0s delay before Success";
        // Do NOT change state here - let Finalizing animation play!
        QTimer::singleShot(1000, this, doTransition);
        return;
    }
    
    // Otherwise transition immediately
    doTransition();
}

void OverlayWidget::setAudioLevel(float level) {
    if (m_state != Recording) return;
    float alpha = 0.2f;
    currentLevel = currentLevel * (1.0f - alpha) + level * alpha;
    for (int i = 0; i < 6; i++) {
        m_barPhases[i] += 0.1f + (float)i * 0.02f;
        float oscillation = 0.8f + std::sin(m_barPhases[i]) * 0.4f;
        float target = 4.0f + (std::sqrt(currentLevel) * 35.0f * oscillation);
        m_barHeights[i] = m_barHeights[i] * 0.6f + target * 0.4f;
    }
    update();
}

void OverlayWidget::setFrequencyBands(const QVector<float> &bands) { Q_UNUSED(bands); }

void OverlayWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_opacity);
    
    // Everything is relative to the center of the 500x100 container
    float centerX = width() / 2.0f;
    float centerY = height() / 2.0f;

    // Use currentWidth for the pill background, centered
    float pillX = centerX - m_currentWidth / 2.0f;
    float pillY = centerY - 24.5f; // Center of 49px height
    QRectF bgRect(pillX + 1, pillY + 1, m_currentWidth - 2, 47);
    
    // RESTORED: Pure White Pill background
    painter.setBrush(Qt::white);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawRoundedRect(bgRect, 23.5, 23.5);

    if (m_state == Recording) {
        // Circle Center is 26px from the left edge of the PILL
        QPointF circleCenter(pillX + 26, centerY);
        
        // Dynamic Indicator: Red Core, Interpolating Ring
        QColor redColor = QColor("#FE2E2E"); // Pure Red from User Specs
        QColor blackColor = QColor(0, 0, 0);   
        
        // Outer ring color interpolates between black and red
        int r = blackColor.red() * (1.0f - m_hoverProgress) + redColor.red() * m_hoverProgress;
        int g = blackColor.green() * (1.0f - m_hoverProgress) + redColor.green() * m_hoverProgress;
        int b = blackColor.blue() * (1.0f - m_hoverProgress) + redColor.blue() * m_hoverProgress;
        QColor ringColor(r, g, b);
        
        // 1. Draw Inner Pulsating Core (Always Red)
        painter.setPen(Qt::NoPen);
        painter.setBrush(redColor);
        
        float baseR = 7.0f * m_pulseScale;
        float finalR = baseR * (1.0f - m_hoverProgress) + 11.0f * m_hoverProgress;
        painter.drawEllipse(circleCenter, finalR, finalR);

        // 2. Draw Outer Ring (Interpolates Black -> Red)
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ringColor, 3));
        painter.drawEllipse(circleCenter, 13.5, 13.5);

        // Bars only show if width is large enough
        if (m_currentWidth > 100) {
            float startX = pillX + 50.0f;
            float spacing = 11.0f;
            
            for (int i = 0; i < 6; i++) {
                float bh = qBound(8.0f, m_barHeights[i], 32.0f);
                float barW = 5.0f; 
                float barH = bh;
                QRectF barRect(startX + (i * spacing), centerY - (barH / 2.0f), barW, barH);
                
                // New Style: White background with Black border (rotated bar aesthetic)
                painter.setBrush(Qt::white);
                painter.setPen(QPen(Qt::black, 2));
                painter.drawRoundedRect(barRect, 2.5, 2.5);
            }
        }
    } 
    else if (m_state == Finalizing) {
        // [FIXED] Larger, More Visible Rotating Loader
         QPointF circleCenter(centerX, centerY);
         painter.translate(circleCenter);
         painter.rotate(m_loaderRotation);
         
         painter.setPen(QPen(Qt::black, 4)); // Thicker stroke for visibility
         painter.drawArc(-16, -16, 32, 32, 0, 270 * 16); // Larger 32px arc
    }
    else if (m_state == Success) {
        // [RESTORED] Success Message Expansion
        float textAlpha = qBound(0.0f, (m_currentWidth - 80.0f) / (m_targetWidth - 80.0f), 1.0f);
        painter.setOpacity(m_opacity * textAlpha);
        
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(12);
        painter.setFont(font);
        
        QRectF textRect(pillX + 50, pillY, m_currentWidth - 60, 49);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_message);
        
        painter.setOpacity(m_opacity);
        QPointF circleCenter(pillX + 26, centerY);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(circleCenter, 13.5, 13.5);
    }
}

void OverlayWidget::mousePressEvent(QMouseEvent *event) { 
    if (m_state == Recording) {
        float centerX = width() / 2.0f;
        float pillX = centerX - m_currentWidth / 2.0f;
        float centerY = height() / 2.0f;
        QRectF pillRect(pillX, centerY - 24.5f, m_currentWidth, 49);
        
        if (pillRect.contains(event->pos())) {
            emit stopOverlay(); 
        }
    }
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *event) {
    float centerX = width() / 2.0f;
    float pillX = centerX - m_currentWidth / 2.0f;
    float centerY = height() / 2.0f;
    QRectF pillRect(pillX, centerY - 24.5f, m_currentWidth, 49);

    bool nowHovered = pillRect.contains(event->pos());
    if (nowHovered != m_isHovered) {
        m_isHovered = nowHovered;
        setCursor(m_isHovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void OverlayWidget::enterEvent(QEnterEvent *event) {
    // We rely on mouseMove for precise pill hover
    QWidget::enterEvent(event);
}

void OverlayWidget::leaveEvent(QEvent *event) {
    m_isHovered = false;
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

void OverlayWidget::keyPressEvent(QKeyEvent *event) {
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && m_state == Recording) {
        emit stopOverlay();
    }
    QWidget::keyPressEvent(event);
}

void OverlayWidget::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
}
