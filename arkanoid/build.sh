#!/bin/bash
# Build Arkanoid using cc65
cd "$(dirname "$0")"

cl65 -t c64 -O -o arkanoid.prg arkanoid.c

if [[ -f arkanoid.prg ]]; then
    echo "Built arkanoid.prg ($(stat -c%s arkanoid.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
