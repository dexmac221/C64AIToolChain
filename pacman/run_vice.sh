#!/bin/bash
# Run Pac-Man in VICE with remote monitor
cd "$(dirname "$0")"

echo "Starting VICE with Pac-Man..."

# Unset Snap/VS Code specific variables that cause conflicts
unset LD_LIBRARY_PATH
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

if [[ ! -f "pacman.prg" ]]; then
    echo "Building first..."
    ./build.sh
fi

# Run x64 with autostart
x64 -remotemonitor -autostart "$PWD/pacman.prg" &
