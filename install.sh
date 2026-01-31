#!/bin/bash
# Toice One-Click Installer 🎙️

set -e

echo "🔮 Installing Toice..."

# 1. Check if Flatpak is installed
if ! command -v flatpak &> /dev/null; then
    echo "❌ Error: Flatpak is not installed. Please install Flatpak first."
    exit 1
fi

# 2. Install the Bundle
# We prefer the release bundle if available, otherwise look locally
if [ -f "toice.flatpak" ]; then
    echo "📦 Found local bundle. Installing..."
    flatpak install --user --reinstall --assumeyes "toice.flatpak"
else
    echo "⬇️  Downloading latest release from GitHub..."
    wget -q --show-progress -O toice.flatpak https://github.com/harshsharma2455/Toice/releases/download/v1.0.0-beta/toice.flatpak
    
    if [ -f "toice.flatpak" ]; then
        echo "📦 Installing downloaded bundle..."
        flatpak install --user --reinstall --assumeyes "toice.flatpak"
        rm toice.flatpak # Cleanup
    else
        echo "❌ Error: Download failed. Please check your internet or the release URL."
        exit 1
    fi
fi

# 3. Success Message & Shortcut Reminder
echo "✅ Installation Complete!"
echo ""
echo "⚡ IMPORTANT: YOU MUST SET THE SHORTCUT MANUALLY"
echo "---------------------------------------------------"
echo "Toice needs a 'Trigger' to work on Wayland."
echo "Go to Settings -> Keyboard -> Custom Shortcuts and add:"
echo ""
echo "  Command:  flatpak run --command=toice-trigger.sh com.toice.app"
echo "  Shortcut: F8 (or your choice)"
echo "---------------------------------------------------"
echo ""
echo "Try running it now with: flatpak run com.toice.app"
