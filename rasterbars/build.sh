#!/bin/bash
cd "$(dirname "$0")"
NAME=$(basename "$PWD")
cl65 -t c64 -O -o ${NAME}.prg ${NAME}.c
if [[ -f ${NAME}.prg ]]; then
    echo "Built ${NAME}.prg ($(stat -c%s ${NAME}.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
