#!/bin/bash
# Build Pac-Man for C64
cl65 -t c64 -C pacman.cfg -o pacman.prg pacman.s
echo "Built pacman.prg"
