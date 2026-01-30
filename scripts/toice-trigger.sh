#!/bin/bash
# toice-trigger.sh
# Sends DBus signals to Toice App

CMD=${1:-toggle}

# Verify if Toice is running
if ! dbus-send --session --dest=org.freedesktop.DBus --type=method_call --print-reply /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep -q "com.toice.app"; then
    echo "Error: Toice is not running."
    exit 1
fi

if [ "$CMD" == "start" ]; then
    dbus-send --session --type=method_call --dest=com.toice.app / com.toice.app.Native.startFromRemote
elif [ "$CMD" == "stop" ]; then
    dbus-send --session --type=method_call --dest=com.toice.app / com.toice.app.Native.stopFromRemote
elif [ "$CMD" == "toggle" ]; then
    dbus-send --session --type=method_call --dest=com.toice.app / com.toice.app.Native.toggleFromRemote
else
    echo "Usage: $0 [start|stop|toggle]"
    exit 1
fi
