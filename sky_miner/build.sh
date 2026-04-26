#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
cl65 -t c64 -O -o sky_miner.prg sky_miner.c

echo "Built sky_miner.prg ($(stat -c%s sky_miner.prg) bytes)"
