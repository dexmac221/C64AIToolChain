#!/bin/bash
set -euo pipefail

# Wrapper: delegate to repo root `run_vice_clean.sh`
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/../run_vice_clean.sh" "$SCRIPT_DIR/pong.prg"
