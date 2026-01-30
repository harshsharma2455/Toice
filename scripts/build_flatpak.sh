#!/bin/bash
# build_flatpak.sh

# Requirements: flatpak, flatpak-builder
if ! command -v flatpak-builder &> /dev/null; then
    echo "Error: flatpak-builder not found."
    echo "Please install it: sudo dnf install flatpak-builder"
    exit 1
fi

echo "Building Flatpak: com.toice.app..."

# Ensure we have the KDE SDK
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install -y flathub org.kde.Sdk//6.6 org.kde.Platform//6.6

# Build
mkdir -p build-flatpak
flatpak-builder --force-clean --user --install --repo=repo build-flatpak "flatpak/com.toice.app.yml"

echo "------------------------------------------------"
echo "Flatpak Build & Install Attempt Finished."
echo "If successful, run with: flatpak run com.toice.app"
echo "------------------------------------------------"
