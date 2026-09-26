#!/bin/bash
#
# deploy.sh - Build in Docker, flash with picotool (forcing BOOTSEL over USB),
# then tail serial output.
#
# Usage:
#   ./scripts/deploy.sh [--no-build] [--monitor SECONDS]
#
# Requires: docker, picotool (brew install picotool), python3 with pyserial
# for --monitor (set PYTHON=/path/to/python if the default python3 lacks it).

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UF2="${PROJECT_ROOT}/build/pico_spec_analyzer.uf2"
PYTHON="${PYTHON:-python3}"
BUILD=true
MONITOR_SECS=0

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) BUILD=false ;;
        --monitor)  MONITOR_SECS="$2"; shift ;;
        *)          echo "unknown argument: $1"; exit 1 ;;
    esac
    shift
done

if $BUILD; then
    echo "==> Building (docker)"
    docker run --rm -v "${PROJECT_ROOT}:/workspace" -w /workspace \
        -e PICO_SDK_PATH=/opt/pico-sdk pico-spec-analyzer:latest \
        bash -c "mkdir -p build && cd build && cmake .. -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release >/dev/null && make -j\$(nproc) 2>&1 | grep -E 'warning|error|Built target pico_spec' ; exit \${PIPESTATUS[0]}"
fi

# picotool talks to the bootrom over USB directly. -f forces a running app
# (stdio_usb reset interface) into BOOTSEL first. This avoids the UF2 drive,
# where copies hang under macOS's FSKit FAT driver.
echo "==> Flashing $(basename "$UF2") via picotool"
picotool load -f -x "$UF2"

if [ "$MONITOR_SECS" -gt 0 ]; then
    for _ in $(seq 1 40); do ls /dev/tty.usbmodem* >/dev/null 2>&1 && break; sleep 0.25; done
    PORT="$(ls /dev/tty.usbmodem* | head -1)"
    echo "==> Monitoring ${PORT} for ${MONITOR_SECS}s"
    "$PYTHON" - "$PORT" "$MONITOR_SECS" <<'EOF'
import serial, sys, time
port, secs = sys.argv[1], float(sys.argv[2])
end = time.time() + secs
while time.time() < end:
    try:
        s = serial.Serial(port, 115200, timeout=0.5)
        break
    except Exception:
        time.sleep(0.2)
while time.time() < end:
    d = s.read(4096)
    if d:
        sys.stdout.write(d.decode(errors="replace")); sys.stdout.flush()
EOF
fi
