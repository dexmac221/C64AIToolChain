#!/bin/bash
# Run Snake 2 in VICE with remote monitor

echo "Starting VICE x64 with Snake 2..."

# Unset Snap/VS Code specific variables that cause conflicts
unset LD_LIBRARY_PATH
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

# Run x64 with autostart
x64 -remotemonitor -autostart "$PWD/snake.prg" &
