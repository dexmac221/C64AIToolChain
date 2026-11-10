#!/bin/bash
# Build FULGOR: assets from assetgen.py, then C and assembly through cl65.
cd "$(dirname "$0")"
python3 assetgen.py || exit 1
python3 music.py || exit 1
rm -f fulgor.prg
if ! cl65 -t c64 -g -Or -Cl ${CFLAGS} ${PROFILE:+--asm-define PROFILE} -C fulgor.cfg -m fulgor.map -Ln fulgor.lbl \
        -o fulgor.prg main.c engine.s game.s sound.s music.s assets.s; then
    echo "Build FAILED (compiler/linker error)"
    exit 1
fi
echo "Built fulgor.prg ($(stat -c%s fulgor.prg) bytes)"
