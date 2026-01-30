#!/bin/bash
# register_toice_shortcut.sh

TRIGGER_SCRIPT="$(pwd)/toice-trigger.sh"
NAME="Toice Toggle"
SHORTCUT="Meta+Z"

echo "Registering shortcut for Toice..."

# 1. Create desktop file
mkdir -p "$HOME/.local/share/applications/"
cat > "$HOME/.local/share/applications/toice-toggle.desktop" <<EOF
[Desktop Entry]
Name=Toice Toggle
Exec="$TRIGGER_SCRIPT" toggle
Type=Application
Terminal=false
Icon=audio-input-microphone
EOF

# 2. Register via kwriteconfig
KWRITE=$(which kwriteconfig6 2>/dev/null || which kwriteconfig5 2>/dev/null)
if [ -n "$KWRITE" ]; then
    $KWRITE --file kglobalshortcutsrc --group "services" --key "toice-toggle.desktop" "$SHORTCUT,none,Toice Toggle"
    dbus-send --session --type=method_call --dest=org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel.reconfigure 2>/dev/null
    update-desktop-database "$HOME/.local/share/applications/"
fi

echo "Toice registered. Use '$SHORTCUT' to toggle recording."
echo "Verified path: $TRIGGER_SCRIPT"
