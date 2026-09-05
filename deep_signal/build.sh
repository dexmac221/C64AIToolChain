#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
python3 assetgen.py
cl65 -t c64 -Or -C deep_signal.cfg -m deep_signal.map -Ln deep_signal.lbl \
    -o deep_signal.prg game.c engine.s fast.s assets.s
printf 'Built DEEP SIGNAL: %s bytes\n' "$(stat -c%s deep_signal.prg)"
