#!/bin/bash
# toice.sh
# Primary launcher for Toice

export QT_QPA_PLATFORM=xcb
# Check if we are in a Flatpak or local dev env
if [ -f "$(dirname "$0")/com.toice.app" ]; then
    # Flatpak or installed location
    cd "$(dirname "$0")"
    exec ./com.toice.app "$@"
else
    # Local dev build directory fallback
    cd "$(dirname "$0")/../build" 2>/dev/null || exit 1
    exec ./com.toice.app "$@"
fi
