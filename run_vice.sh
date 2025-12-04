#!/bin/bash
# Run VICE emulator with remote monitor enabled
# This script handles environment issues when running from VS Code

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Clear problematic environment variables that can cause library conflicts
unset GTK_PATH
unset GIO_EXTRA_MODULES
unset LD_LIBRARY_PATH

# Get PRG file path (default to snake/snake.prg)
PRG_FILE="${1:-snake/snake.prg}"

# Convert to absolute path if relative
if [[ ! "$PRG_FILE" = /* ]]; then
    PRG_FILE="$SCRIPT_DIR/$PRG_FILE"
fi

# Check if file exists
if [[ ! -f "$PRG_FILE" ]]; then
    echo "Error: File not found: $PRG_FILE"
    exit 1
fi

echo "Loading: $PRG_FILE"

# Try different VICE executables in order of preference
if command -v x64sc &> /dev/null; then
    exec x64sc -remotemonitor "$PRG_FILE"
elif command -v x64 &> /dev/null; then
    exec x64 -remotemonitor "$PRG_FILE"
else
    echo "Error: VICE emulator not found. Please install vice package."
    echo "  Ubuntu/Debian: sudo apt install vice"
    echo "  Fedora: sudo dnf install vice"
    exit 1
fi
