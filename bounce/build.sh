#!/bin/bash
# Build Bouncing Ball using cc65
cd "$(dirname "$0")"

cl65 -t c64 -O -o bounce.prg bounce.c

if [[ -f bounce.prg ]]; then
    echo "Built bounce.prg ($(stat -c%s bounce.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
