#include "globalshortcut.h"

// 1. Include ALL Qt headers first
#include <QDebug>
#include <QGuiApplication>
#include "databasemanager.h"

// 2. Include X11 headers
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

// 3. Undefine X11 macros that conflict with Qt/Standard C++
#undef None
#undef Bool
#undef Success
#undef Status
#undef CursorShape
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef FocusOut
#undef FontChange

#include <QMessageBox>
#include <cstdio>

static bool g_x11ErrorOccurred = false;
GlobalShortcut::GlobalShortcut(QObject *parent) : QObject(parent)
{
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        qCritical() << "X11 ERROR: Could not open display! Global shortcut will fail.";
    } else {
        qDebug() << "X11 Display opened successfully for Global Shortcut.";
    }
}

GlobalShortcut::~GlobalShortcut()
{
    if (m_display) {
        XCloseDisplay((Display*)m_display);
    }
}

bool GlobalShortcut::registerShortcut()
{
    if (!m_display) return false;

    Display* dpy = (Display*)m_display;
    Window root = DefaultRootWindow(dpy);

    // Try both lowercase 'z' and uppercase 'Z' keysyms
    m_keycode = XKeysymToKeycode(dpy, XStringToKeysym("z"));
    m_modifiers = Mod4Mask; // Super/Win key

    qDebug() << "X11: Grabbing Keycode" << m_keycode << "with Super (Mod4Mask)";

    // Grab all common modifier combinations for NumLock/CapsLock/ScrollLock
    // X11 Key codes for mods: Mod2Mask = NumLock, LockMask = CapsLock
    unsigned int modifiers[] = {0, Mod2Mask, LockMask, Mod5Mask, Mod2Mask|LockMask, Mod2Mask|Mod5Mask, LockMask|Mod5Mask, Mod2Mask|LockMask|Mod5Mask};
    
    // Set temporary Error Handler to catch BadAccess (key already grabbed)
    // Set temporary Error Handler to catch BadAccess (key already grabbed)
    g_x11ErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler([](Display *d, XErrorEvent *e) -> int {
        char buffer[1024];
        XGetErrorText(d, e->error_code, buffer, 1024);
        fprintf(stderr, "[[X11]] ERROR: %s (ErrorCode %d)\n", buffer, e->error_code);
        if (e->error_code == BadAccess) {
            g_x11ErrorOccurred = true;
        }
        return 0;
    });

    bool success = true;
    for (unsigned int mod : modifiers) {
        if (XGrabKey(dpy, m_keycode, m_modifiers | mod, root, True, GrabModeAsync, GrabModeAsync)) {
             // success
        }
    }
    
    // BACKUP SHORTCUT: Ctrl+Shift+Z (to test if Super is key-blocked)
    KeyCode backupCode = XKeysymToKeycode(dpy, XStringToKeysym("z"));
    unsigned int backupMod = ControlMask | ShiftMask;
    for (unsigned int mod : modifiers) {
        XGrabKey(dpy, backupCode, backupMod | mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    
    // Sync to flush errors
    XSync(dpy, False);
    
    // Restore handler
    XSetErrorHandler(oldHandler);

    if (g_x11ErrorOccurred) {
        fprintf(stderr, "[[X11]] INFO: Native Global Shortcut (Super+Z) is blocked by the OS/KDE.\n");
        fprintf(stderr, "[[X11]] INFO: Falling back to external DBus trigger mode.\n");
        return false;
    }

    // Enable Detectable Auto-Repeat
    int supported;
    XkbSetDetectableAutoRepeat(dpy, True, &supported);
    if (!supported) {
        fprintf(stderr, "[[X11]] Warning: Detectable Auto-Repeat not supported.\n");
    }
    
    fprintf(stderr, "[[X11]] SUCCESS: Registered Global Shortcut Super+Z\n");
    
    qGuiApp->installNativeEventFilter(this);
    return true;
}

void GlobalShortcut::setShortcut(Preset preset)
{
    if (!m_display) return;
    Display* dpy = (Display*)m_display;
    Window root = DefaultRootWindow(dpy);
    
    // Ungrab everything first
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    
    // Define Key and Modifier based on Preset
    const char* keyStr = "z";
    unsigned int primaryMod = Mod4Mask; // Default Super
    
    switch (preset) {
        case SuperZ:     keyStr = "z"; primaryMod = Mod4Mask; break;
        case CtrlSpace:  keyStr = "space"; primaryMod = ControlMask; break;
        case SuperSpace: keyStr = "space"; primaryMod = Mod4Mask; break;
        case CtrlAltZ:   keyStr = "z"; primaryMod = ControlMask | Mod1Mask; break;
    }
    
    m_keycode = XKeysymToKeycode(dpy, XStringToKeysym(keyStr));
    m_modifiers = primaryMod;
    
    qDebug() << "Re-registering shortcut for preset:" << preset << "Key:" << keyStr;

    // Grab with all lock modifiers (Caps, Num, etc)
    unsigned int modifiers[] = {0, Mod2Mask, LockMask, Mod5Mask, Mod2Mask|LockMask, Mod2Mask|Mod5Mask, LockMask|Mod5Mask, Mod2Mask|LockMask|Mod5Mask};
    
    bool success = true;
    
    // Set Error Handler
    g_x11ErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler([](Display *d, XErrorEvent *e) -> int {
        if (e->error_code == BadAccess) g_x11ErrorOccurred = true;
        return 0;
    });

    for (unsigned int mod : modifiers) {
        XGrabKey(dpy, m_keycode, m_modifiers | mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    
    XSync(dpy, False);
    XSetErrorHandler(oldHandler);

    if (g_x11ErrorOccurred) {
        qDebug() << "Failed to grab new shortcut keys (Occupied).";
    } else {
        qDebug() << "Shortcut updated successfully.";
        DatabaseManager::instance().setSetting("shortcut_preset", QString::number(preset));
    }
    
    // Also re-grab backup if needed? For now, stick to primary only to avoid conflicts.
}

bool GlobalShortcut::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "xcb_generic_event_t") return false;

    struct xcb_generic_event_t {
        uint8_t response_type;
        uint8_t pad0;
        uint16_t sequence;
        uint32_t pad[7];
        uint32_t full_sequence;
    };
    
    auto *event = static_cast<xcb_generic_event_t *>(message);
    uint8_t type = event->response_type & ~0x80;

    // xcb event types: 2 = KeyPress, 3 = KeyRelease
    if (type == 2 || type == 3) {
        struct xcb_key_press_event_t {
            uint8_t response_type;
            uint8_t detail;
            uint16_t sequence;
            uint32_t time;
            uint32_t root;
            uint32_t event;
            uint32_t child;
            uint16_t root_x;
            uint16_t root_y;
            uint16_t event_x;
            uint16_t event_y;
            uint16_t state;
            uint8_t same_screen;
            uint8_t pad0;
        };
        auto *kp = reinterpret_cast<xcb_key_press_event_t *>(message);
        
        if (kp->detail == m_keycode && (kp->state & Mod4Mask)) {
            if (type == 2) { // Press
                if (!m_isPressed) {
                    qDebug() << "NATIVE HOLD: Start (Super+Z)";
                    m_isPressed = true;
                    emit keyPressed();
                }
            } else { // Release
                if (m_isPressed) {
                    qDebug() << "NATIVE HOLD: Stop (Super+Z)";
                    m_isPressed = false;
                    emit keyReleased();
                }
            }
            return true;
        }
        
        // Check Backup: Ctrl + Shift + Z
        if (kp->detail == m_keycode && (kp->state & ControlMask) && (kp->state & ShiftMask)) {
             if (type == 2) { // Press
                if (!m_isPressed) {
                    qDebug() << "BACKUP SHORTCUT (Ctrl+Shift+Z) PRESSED!";
                    m_isPressed = true;
                    emit keyPressed();
                }
            } else { // Release
                if (m_isPressed) {
                    qDebug() << "BACKUP SHORTCUT RELEASED!";
                    m_isPressed = false;
                    emit keyReleased();
                }
            }
            return true;
        }
    }

    return false;
}
