#!/bin/bash
# Build Pong C version using cc65
cd "$(dirname "$0")"

# Compile and link
cl65 -t c64 -O -o pong.prg pong.c

if [[ -f pong.prg ]]; then
    echo "Built pong.prg ($(stat -c%s pong.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
