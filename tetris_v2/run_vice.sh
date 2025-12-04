#!/bin/bash
# Wrapper to run VICE x64 inside VS Code Snap environment
unset LD_LIBRARY_PATH
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
x64 -remotemonitor -autostart "$DIR/tetris.prg" &
