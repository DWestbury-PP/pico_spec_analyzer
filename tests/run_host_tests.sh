#!/bin/bash
# Build and run the portable DSP tests on the host (no Pico SDK needed).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)/test_dsp"
cc -std=c11 -O2 -Wall -Wextra -DHOST_TEST -I"$ROOT/include" \
    "$ROOT/tests/test_dsp.c" "$ROOT/src/audio/dsp.c" "$ROOT/src/audio/fft_q.c" \
    -lm -o "$OUT"
"$OUT"
