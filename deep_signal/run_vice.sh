#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
export VICE_EXTRA_ARGS="${VICE_EXTRA_ARGS:-} -pal"
source ../run_vice_generic.sh deep_signal.prg
wait "$!"
