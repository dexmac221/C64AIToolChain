#!/bin/bash
cd "$(dirname "$0")"
python3 assetgen.py || exit 1
rm -f ironvein.prg
if ! cl65 -t c64 -Or -C ironvein.cfg -m ironvein.map -Ln ironvein.lbl -o ironvein.prg ironvein.c irq.s scroll.s edges.s actors.s physics.s; then
    echo "Build FAILED (compiler/linker error)"
    exit 1
fi
echo "Built ironvein.prg ($(stat -c%s ironvein.prg) bytes)"
