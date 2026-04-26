#!/bin/bash
set -euo pipefail

# Backward-compatible root VICE launcher.
# Keeps existing project wrappers working while delegating to the generic runner.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default to the root compatibility artifact produced by build.sh.
PRG_FILE="${1:-$SCRIPT_DIR/snake.prg}"

exec "$SCRIPT_DIR/run_vice_generic.sh" "$PRG_FILE"
