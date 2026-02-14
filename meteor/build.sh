#!/bin/bash
# Build Meteor Storm using cc65
cd "$(dirname "$0")"

cl65 -t c64 -C meteor.cfg -O -o meteor.prg meteor.c

if [[ -f meteor.prg ]]; then
    echo "Built meteor.prg ($(stat -c%s meteor.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
