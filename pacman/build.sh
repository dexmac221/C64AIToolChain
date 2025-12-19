#!/bin/bash
set -euo pipefail

# Build Pac-Man for C64
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cl65 -t c64 \
	-C "$SCRIPT_DIR/pacman.cfg" \
	-o "$SCRIPT_DIR/pacman.prg" \
	"$SCRIPT_DIR/pacman.s"

echo "Built pacman.prg"
