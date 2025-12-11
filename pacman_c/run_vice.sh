#!/bin/bash
# Run Pac-Man in VICE with remote monitor
cd "$(dirname "$0")"
x64 -remotemonitor pacman.prg
