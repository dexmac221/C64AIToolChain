#!/bin/bash
# Run the matrix project in VICE
cd "$(dirname "$0")"
# Unset environment variables that might conflict with Snap
unset GTK_PATH
unset GDK_PIXBUF_MODULE_FILE
x64sc -autostart matrix.prg
