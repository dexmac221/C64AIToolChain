#!/bin/bash
cd "$(dirname "$0")"
cl65 -t c64 -O -o starfield.prg starfield.c
if [[ -f starfield.prg ]]; then
    echo "Built starfield.prg ($(stat -c%s starfield.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
