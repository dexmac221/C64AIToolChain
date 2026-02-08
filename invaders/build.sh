#!/bin/bash
# Build Space Invaders using cc65
cd "$(dirname "$0")"

cl65 -t c64 -O -o invaders.prg invaders.c

if [[ -f invaders.prg ]]; then
    echo "Built invaders.prg ($(stat -c%s invaders.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
