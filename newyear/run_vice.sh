#!/bin/bash
# Run the New Year demo in VICE

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Build if needed
if [ ! -f newyear.prg ] || [ main.c -nt newyear.prg ]; then
    echo "Building newyear.prg..."
    make
fi

# Unset Snap/VS Code specific variables that cause conflicts
unset LD_LIBRARY_PATH
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

# Run in VICE
x64sc -remotemonitor -remotemonitoraddress ip4://127.0.0.1:6510 -autostart newyear.prg
