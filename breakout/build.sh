#!/bin/bash
cd "$(dirname "$0")"
cl65 -t c64 -O -o breakout.prg breakout.c
if [[ -f breakout.prg ]]; then
    echo "Built breakout.prg ($(stat -c%s breakout.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
