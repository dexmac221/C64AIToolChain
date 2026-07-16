#!/bin/bash
cd "$(dirname "$0")"
python3 assetgen.py || exit 1
cl65 -t c64 -O -C ionrift.cfg -o ionrift.prg ionrift.c irq.s scroll.s
if [[ -f ionrift.prg ]]; then
    echo "Built ionrift.prg ($(stat -c%s ionrift.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
