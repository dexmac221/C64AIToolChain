#!/bin/bash
# Wrapper to run VICE x64 inside VS Code Snap environment
# Unsets variables that cause library conflicts

echo "Starting VICE x64 with cleaned environment..."

# Unset Snap/VS Code specific variables that cause conflicts
unset LD_LIBRARY_PATH
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

# Run x64
# We use autostart for the PRG
x64 -remotemonitor -autostart "$PWD/snake.prg" &
