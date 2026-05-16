#!/bin/bash
# Run Frogger in VICE with remote monitor enabled
cd "$(dirname "$0")"
unset GTK_PATH
unset GDK_PIXBUF_MODULE_FILE

if command -v x64sc &>/dev/null; then
    x64sc +warp +autostart-warp -remotemonitor frogger.prg $VICE_EXTRA_ARGS
elif command -v x64 &>/dev/null; then
    x64 +warp +autostart-warp -remotemonitor frogger.prg $VICE_EXTRA_ARGS
else
    echo "VICE not found (tried x64sc, x64)"
    exit 1
fi
