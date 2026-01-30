#!/bin/bash
# toggle_recording.sh
# Sends a DBus message to the WhisperLiveNative application to start/stop recording.

# You can map this script to any global shortcut (e.g., Win+Shift+Z) 
# in your KDE/GNOME/System Settings.

qdbus com.whisper.live / com.whisper.live.Native.toggleFromRemote
