#!/bin/sh
# Build libsmb2 as a static library for the given NextUI PLATFORM, skipping
# the work if it was already built. Must run with the right compiler on
# PATH: natively for PLATFORM=desktop, inside the Docker cross-compile
# toolchain container (see ../run-docker.sh) for tg5040/tg5050 -- this
# script itself never invokes Docker.
set -e

PLATFORM="$1"
[ -z "$PLATFORM" ] && { echo "usage: $0 <desktop|tg5040|tg5050>" >&2; exit 1; }

HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/libsmb2"
BUILD="$HERE/build/libsmb2-$PLATFORM"

if [ -f "$BUILD/lib/libsmb2.a" ]; then
    exit 0
fi

CMAKE_ARGS="-DBUILD_SHARED_LIBS=OFF -DENABLE_EXAMPLES=OFF -DENABLE_LIBDCERPC=OFF -DCMAKE_BUILD_TYPE=Release"
if [ "$PLATFORM" != "desktop" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$HERE/libsmb2-toolchain.cmake"
fi

mkdir -p "$BUILD"
cd "$BUILD"
cmake $CMAKE_ARGS "$SRC"
make -j"$(nproc 2>/dev/null || echo 2)"
