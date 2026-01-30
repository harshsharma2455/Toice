#ifndef GLOBALSHORTCUT_H
#define GLOBALSHORTCUT_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>

class GlobalShortcut : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalShortcut(QObject *parent = nullptr);
    ~GlobalShortcut();

    bool registerShortcut();
    
    // QAbstractNativeEventFilter interface
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

    enum Preset {
        SuperZ,
        CtrlSpace,
        SuperSpace,
        CtrlAltZ
    };
    Q_ENUM(Preset)

    void setShortcut(Preset preset);

signals:
    void keyPressed();
    void keyReleased();

private:
    void* m_display;
    unsigned int m_keycode;
    unsigned int m_modifiers;
    bool m_isPressed = false;
};

#endif // GLOBALSHORTCUT_H
