#!/bin/bash
set -euo pipefail

# Root build entrypoint (used by VS Code tasks)
# Default: build Snake and copy artifact to repo root as snake.prg
# Usage:
#   ./build.sh                # builds snake/
#   ./build.sh snake2         # builds snake2/
#   ./build.sh tetris_v1      # builds tetris_v1/
#   ./build.sh pacman_c       # builds pacman_c/
#   ./build.sh <dir> [prg]    # optional explicit prg relative to <dir>

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="${1:-snake}"

PROJECT_DIR="$ROOT_DIR/$PROJECT"
if [[ ! -d "$PROJECT_DIR" ]]; then
  echo "Error: project directory not found: $PROJECT_DIR" >&2
  exit 1
fi

if [[ ! -f "$PROJECT_DIR/build.sh" ]]; then
  echo "Error: missing build script: $PROJECT_DIR/build.sh" >&2
  exit 1
fi

# Build
( cd "$PROJECT_DIR" && bash ./build.sh )

# Determine output PRG inside project
PRG_IN_PROJECT="${2:-}"
if [[ -z "$PRG_IN_PROJECT" ]]; then
  # Common convention: snake2 builds snake.prg, others build <project>.prg
  if [[ -f "$PROJECT_DIR/$PROJECT.prg" ]]; then
    PRG_IN_PROJECT="$PROJECT.prg"
  else
    PRG_IN_PROJECT="$(ls -1 "$PROJECT_DIR"/*.prg 2>/dev/null | head -n 1 | xargs -r basename)"
  fi
fi

if [[ -z "$PRG_IN_PROJECT" || ! -f "$PROJECT_DIR/$PRG_IN_PROJECT" ]]; then
  echo "Error: could not find .prg output in $PROJECT_DIR" >&2
  exit 1
fi

# For compatibility with existing VS Code task, always expose a root-level snake.prg
cp -f "$PROJECT_DIR/$PRG_IN_PROJECT" "$ROOT_DIR/snake.prg"

# Also expose map if present (useful when building snake/ or tetris)
if [[ -f "$PROJECT_DIR/${PRG_IN_PROJECT%.prg}.map" ]]; then
  cp -f "$PROJECT_DIR/${PRG_IN_PROJECT%.prg}.map" "$ROOT_DIR/snake.map"
fi

SIZE=$(stat -c%s "$ROOT_DIR/snake.prg" 2>/dev/null || wc -c < "$ROOT_DIR/snake.prg")
echo "Built $PROJECT/$PRG_IN_PROJECT -> snake.prg ($SIZE bytes)"
