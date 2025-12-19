#!/bin/bash
set -euo pipefail

# Wrapper: delegate to repo root `run_vice.sh`
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -f "$SCRIPT_DIR/pacman.prg" ]]; then
    echo "Building first..."
    (cd "$SCRIPT_DIR" && bash ./build.sh)
fi

exec "$SCRIPT_DIR/../run_vice.sh" "$SCRIPT_DIR/pacman.prg"
