#!/bin/bash
# Build Pac-Man C version using cc65
cd "$(dirname "$0")"

# Compile and link
cl65 -t c64 -O -o pacman.prg pacman.c

if [[ -f pacman.prg ]]; then
    echo "Built pacman.prg ($(stat -c%s pacman.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
