#!/bin/bash
cd "$(dirname "$0")"
cl65 -t c64 -O -o boulderdash.prg boulderdash.c
if [[ -f boulderdash.prg ]]; then
    echo "Built boulderdash.prg ($(stat -c%s boulderdash.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
