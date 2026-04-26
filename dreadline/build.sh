#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
python3 spritegen.py
python3 bggen.py
cl65 -t c64 -O -o dreadline.prg dreadline.c fastscroll.s

echo "Built dreadline.prg ($(stat -c%s dreadline.prg) bytes)"
