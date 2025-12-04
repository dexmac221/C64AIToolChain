#!/bin/bash
# Take a screenshot from VICE via remote monitor
# Usage: ./screenshot.sh [filename] [format]
# Format: 0=BMP (default), 1=PCX, 2=PNG, 3=GIF, 4=IFF

FILENAME="${1:-screenshot}"
FORMAT="${2:-2}"  # Default to PNG

# Add extension if not present
case $FORMAT in
    0) EXT="bmp" ;;
    1) EXT="pcx" ;;
    2) EXT="png" ;;
    3) EXT="gif" ;;
    4) EXT="iff" ;;
    *) EXT="png"; FORMAT=2 ;;
esac

# Add extension if not already present
if [[ ! "$FILENAME" =~ \.$EXT$ ]]; then
    FILENAME="${FILENAME}.${EXT}"
fi

# Get absolute path
if [[ ! "$FILENAME" = /* ]]; then
    FILENAME="$(pwd)/$FILENAME"
fi

echo "Taking screenshot: $FILENAME"

# Send screenshot command to VICE monitor (with sleep to allow response)
(echo "screenshot \"$FILENAME\" $FORMAT"; sleep 1; echo "x") | nc localhost 6510 >/dev/null 2>&1 &
NC_PID=$!

# Wait for nc to finish or timeout
sleep 2
kill $NC_PID 2>/dev/null

if [[ -f "$FILENAME" ]]; then
    echo "Screenshot saved: $FILENAME"
    ls -la "$FILENAME"
else
    echo "Error: Screenshot not created. Is VICE running with -remotemonitor?"
fi
