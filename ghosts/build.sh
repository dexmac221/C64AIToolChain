#!/bin/bash
cd "$(dirname "$0")"
python3 assetgen.py || exit 1
rm -f ghosts.prg
if ! cl65 -t c64 -O -C ghosts.cfg -m ghosts.map -o ghosts.prg ghosts.c irq.s scroll.s; then
    echo "Build FAILED (compiler/linker error)"
    exit 1
fi
echo "Built ghosts.prg ($(stat -c%s ghosts.prg) bytes)"
