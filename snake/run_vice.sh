#!/bin/bash
set -euo pipefail

# Wrapper: delegate to repo root `run_vice.sh` (single source of truth)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/../run_vice.sh" "$SCRIPT_DIR/snake.prg"
