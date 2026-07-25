#!/bin/bash
cd "$(dirname "$0")"
python3 assetgen.py || exit 1
rm -f ionrift.prg
if ! cl65 -t c64 -O -C ionrift.cfg -m ionrift.map -o ionrift.prg ionrift.c irq.s scroll.s; then
    echo "Build FAILED (compiler/linker error)"
    exit 1
fi
echo "Built ionrift.prg ($(stat -c%s ionrift.prg) bytes)"
