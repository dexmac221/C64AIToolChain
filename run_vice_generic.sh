#!/bin/bash
# Wrapper to run VICE x64 inside VS Code Snap environment
# Unsets variables that cause library conflicts

echo "Starting VICE x64 with cleaned environment..."

# Unset Snap/VS Code specific variables that cause conflicts
unset LD_LIBRARY_PATH
unset GDK_PIXBUF_MODULE_FILE
unset GTK_PATH
unset GIO_MODULE_DIR
unset GTK_IM_MODULE_FILE
unset LOCPATH

PRG_FILE=$1
if [ -z "$PRG_FILE" ]; then
    echo "Usage: $0 <prg_file>"
    exit 1
fi

read -r -a EXTRA_ARGS <<< "${VICE_EXTRA_ARGS:-}"
VICE_SPEED="${VICE_SPEED:-100}"
if ! [[ "$VICE_SPEED" =~ ^[1-9][0-9]*$ ]]; then
    echo "VICE_SPEED must be a positive percentage (100 = normal, 0 is unlimited)." >&2
    exit 1
fi

# Run x64 at normal speed. The +warp flags explicitly disable VICE warp mode,
# including the autostart warp path that can otherwise peg the host CPU.
echo "VICE speed guard: warp=off autostart-warp=off speed=${VICE_SPEED}% monitor=127.0.0.1:6510"
x64 "${EXTRA_ARGS[@]}" +warp +autostart-warp -speed "$VICE_SPEED" \
    -remotemonitor -remotemonitoraddress ip4://127.0.0.1:6510 \
    -autostart "$PRG_FILE" &
