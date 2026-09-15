#!/bin/sh
# Build and run Samba Sync natively on the host (Linux/macOS) for local
# development -- no device, no Docker, no cross-compiler.
#
# Requires the NextUI `desktop` workspace to be prepared first (fake SD root +
# libmsettings.so under /var/tmp/nextui). This script does that setup once,
# the first time it's run; if any step of it fails, nothing is marked done,
# so the next run retries the whole setup from scratch.
#
# This script is local-only (not shipped in the pak). Usage:
#   sh build-desktop.sh            # build, then launch
#   sh build-desktop.sh --build    # build only, don't run
set -e

PLATFORM=desktop
HERE=$(cd "$(dirname "$0")" && pwd)

LINK="$HERE/.nextui-workspace"
if [ ! -e "$LINK" ]; then
    echo "ERROR: $LINK is missing." >&2
    echo "Create it pointing at your NextUI workspace directory, e.g.:" >&2
    echo "  ln -s /path/to/nextui/workspace \"$LINK\"" >&2
    exit 1
fi
NEXTUI_WORKSPACE=$(cd "$LINK" 2>/dev/null && pwd -P) || {
    echo "ERROR: $LINK exists but is not a directory (or is unreadable)." >&2
    exit 1
}
NEXTUI_ROOT=$(cd "$NEXTUI_WORKSPACE/.." && pwd -P)

# --- native "toolchain" (see nextui/makefile.native) -------------------------
case "$(uname -s)" in
    Linux)
        export CROSS_COMPILE=/usr/bin/
        export PREFIX=/usr
        ;;
    Darwin)
        export CROSS_COMPILE=/usr/local/bin/
        export PREFIX=/opt/homebrew
        ;;
    *)
        echo "Unsupported host OS: $(uname -s)" >&2
        exit 1
        ;;
esac
export PREFIX_LOCAL=/var/tmp/nextui
export UNION_PLATFORM=$PLATFORM

# --- one-time desktop workspace setup ---------------------------------------
# Marker is only written after every step below succeeds, so a failure
# (e.g. declined sudo, missing brew package) leaves it unset and the full
# setup re-runs next time instead of silently skipping the broken step.
SETUP_MARKER="$PREFIX_LOCAL/.desktop_setup_ok"
if [ ! -f "$SETUP_MARKER" ]; then
    echo "Preparing NextUI desktop workspace (one-time setup)..."

    if [ "$(uname -s)" = "Darwin" ] && [ ! -x /usr/local/bin/gcc ]; then
        sudo "$NEXTUI_ROOT/workspace/desktop/macos_create_gcc_symlinks.sh"
    fi

    "$NEXTUI_ROOT/workspace/desktop/prepare_fake_sd_root.sh"

    make -C "$NEXTUI_ROOT" build PLATFORM="$PLATFORM"

    mkdir -p "$PREFIX_LOCAL"
    touch "$SETUP_MARKER"
    echo "Desktop workspace setup complete."
fi

# --- libsmb2 (static, native) ------------------------------------------
sh "$HERE/lib/build-libsmb2.sh" "$PLATFORM"

# --- build --------------------------------------------------------------
# This repo isn't physically nested inside the NextUI checkout (unlike the
# Docker cross-compile, where run-docker.sh bind-mounts things that way), so
# point the Makefile's NEXTUI_ALL/NEXTUI_PLATFORM overrides at the resolved
# workspace instead of relying on its "../../all" default.
make -C "$HERE/src" PLATFORM="$PLATFORM" \
    NEXTUI_ALL="$NEXTUI_WORKSPACE/all" \
    NEXTUI_PLATFORM="$NEXTUI_WORKSPACE/$PLATFORM/platform"

[ "$1" = "--build" ] && exit 0

# --- run ----------------------------------------------------------------
# The on-device paths are hardcoded to SDCARD_PATH=/var/tmp/nextui/sdcard, so
# create the userdata tree the app expects and launch from the project root.
export LD_LIBRARY_PATH="$PREFIX_LOCAL/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$PREFIX_LOCAL/lib:$DYLD_LIBRARY_PATH"
mkdir -p /var/tmp/nextui/sdcard/.userdata/shared
mkdir -p /var/tmp/nextui/sdcard/.userdata/desktop

cd "$HERE"
exec ./bin/desktop/sambasync.elf
