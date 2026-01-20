#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAME=$(basename "$SCRIPT_DIR")
exec "$SCRIPT_DIR/../run_vice_clean.sh" "$SCRIPT_DIR/${NAME}.prg"
