#!/bin/bash
# Build Frogger using cc65
cd "$(dirname "$0")"

cl65 -t c64 -C frogger.cfg -O -o frogger.prg frogger.c

if [[ -f frogger.prg ]]; then
    echo "Built frogger.prg ($(stat -c%s frogger.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
