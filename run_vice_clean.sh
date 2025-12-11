#!/bin/bash
# Run VICE x64 with clean environment (fixes VS Code snap conflicts)
# Usage: ./run_vice_clean.sh <program.prg>

# Clear VS Code snap-polluted environment variables
unset GTK_PATH
unset GTK_EXE_PREFIX
unset GTK_IM_MODULE_FILE
unset GIO_MODULE_DIR
unset GSETTINGS_SCHEMA_DIR
unset LOCPATH

# Restore original XDG paths if available
if [[ -n "$XDG_DATA_DIRS_VSCODE_SNAP_ORIG" ]]; then
    export XDG_DATA_DIRS="$XDG_DATA_DIRS_VSCODE_SNAP_ORIG"
fi

# Run VICE with remote monitor enabled
# Use -autostart for .prg files
exec x64 -remotemonitor -autostart "$@"
