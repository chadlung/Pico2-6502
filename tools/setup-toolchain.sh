#!/usr/bin/env bash
# Download and build everything needed to build the Pico 6502 firmware: the Arm
# GNU toolchain, CMake, the Pico SDK, picotool and the 64tass assembler.
#
#   tools/setup-toolchain.sh [DIR]      DIR defaults to ../6502-Pico-Build
#
# Everything is installed inside DIR, together with DIR/env.sh, which sets up
# PATH and PICO_SDK_PATH.  Nothing is installed system-wide.  Re-running skips
# whatever is already there.
set -euo pipefail

PROJECT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="${1:-$PROJECT/../6502-Pico-Build}"
mkdir -p "$DIR"
DIR="$(cd "$DIR" && pwd)"
cd "$DIR"

ARM_VERSION=14.2.rel1
CMAKE_VERSION=3.31.6
PICO_SDK_VERSION=2.1.1          # picotool must use the same version
TASS_VERSION=1.60.3243
JOBS="$(nproc)"

step() { printf '\n== %s\n' "$*"; }

if [ ! -x arm-gnu-toolchain/bin/arm-none-eabi-gcc ]; then
    step "Arm GNU toolchain $ARM_VERSION"
    name="arm-gnu-toolchain-$ARM_VERSION-x86_64-arm-none-eabi"
    curl -fL -o arm.tar.xz "https://developer.arm.com/-/media/Files/downloads/gnu/$ARM_VERSION/binrel/$name.tar.xz"
    rm -rf arm-gnu-toolchain "$name"
    tar xf arm.tar.xz
    mv "$name" arm-gnu-toolchain
    rm arm.tar.xz
fi

if [ ! -x cmake/bin/cmake ]; then
    step "CMake $CMAKE_VERSION"
    name="cmake-$CMAKE_VERSION-linux-x86_64"
    curl -fL -o cmake.tar.gz "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/$name.tar.gz"
    rm -rf cmake "$name"
    tar xzf cmake.tar.gz
    mv "$name" cmake
    rm cmake.tar.gz
fi

export PATH="$DIR/arm-gnu-toolchain/bin:$DIR/cmake/bin:$PATH"

if [ ! -f pico-sdk/pico_sdk_init.cmake ]; then
    step "Pico SDK $PICO_SDK_VERSION"
    rm -rf pico-sdk
    git clone -b "$PICO_SDK_VERSION" --depth 1 https://github.com/raspberrypi/pico-sdk.git
    git -C pico-sdk submodule update --init --depth 1 lib/tinyusb
fi

if ! find picotool-install -name picotoolConfig.cmake 2>/dev/null | grep -q .; then
    step "picotool $PICO_SDK_VERSION"
    rm -rf picotool picotool-install
    git clone -b "$PICO_SDK_VERSION" --depth 1 https://github.com/raspberrypi/picotool.git
    cmake -S picotool -B picotool/build -DCMAKE_BUILD_TYPE=Release \
        -DPICO_SDK_PATH="$DIR/pico-sdk" \
        -DCMAKE_INSTALL_PREFIX="$DIR/picotool-install" -DPICOTOOL_FLAT_INSTALL=1
    cmake --build picotool/build -j"$JOBS"
    cmake --install picotool/build
fi

if [ ! -x bin/64tass ]; then
    step "64tass $TASS_VERSION"
    rm -rf "64tass-$TASS_VERSION-src"
    curl -fL -o 64tass.zip "https://sourceforge.net/projects/tass64/files/source/64tass-$TASS_VERSION-src.zip/download"
    unzip -q 64tass.zip
    rm 64tass.zip
    make -C "64tass-$TASS_VERSION-src" -j"$JOBS"
    mkdir -p bin
    cp "64tass-$TASS_VERSION-src/64tass" bin/
fi

step "Writing $DIR/env.sh"
cat > env.sh <<'EOF'
# Build environment for the Pico 6502 firmware (written by setup-toolchain.sh).
# Load it into a shell with:  . /path/to/6502-Pico-Build/env.sh
BUILD_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PICO_SDK_PATH="$BUILD_ROOT/pico-sdk"
export picotool_DIR="$(dirname "$(find "$BUILD_ROOT/picotool-install" -name picotoolConfig.cmake -print -quit)")"
export PATH="$BUILD_ROOT/arm-gnu-toolchain/bin:$BUILD_ROOT/cmake/bin:$BUILD_ROOT/bin:$picotool_DIR:$PATH"
EOF

step "Done"
echo "Build the firmware with: $PROJECT/tools/build-firmware.sh $DIR"
