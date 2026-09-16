#!/usr/bin/env bash
# Build the Pico 6502 firmware with the tools installed by setup-toolchain.sh.
#
#   tools/build-firmware.sh [DIR]      DIR defaults to ../6502-Pico-Build
#
# The firmware is written to DIR/build/pico6502.uf2.
set -euo pipefail

PROJECT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="$(cd "${1:-$PROJECT/../6502-Pico-Build}" && pwd)"

if [ ! -f "$DIR/env.sh" ]; then
    echo "error: $DIR/env.sh not found; run tools/setup-toolchain.sh first" >&2
    exit 1
fi
. "$DIR/env.sh"

cmake -S "$PROJECT" -B "$DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$DIR/build" -j"$(nproc)"

echo
echo "Firmware: $DIR/build/pico6502.uf2"
