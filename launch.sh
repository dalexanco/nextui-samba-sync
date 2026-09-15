#!/bin/sh
DIR="$(dirname "$0")"
cd "$DIR"

[ -z "$PLATFORM" ] && PLATFORM="tg5040"

export LD_LIBRARY_PATH="$DIR:$DIR/bin:$DIR/bin/$PLATFORM:$LD_LIBRARY_PATH:/usr/bin"

"$DIR/bin/$PLATFORM/sambasync.elf" &> "$LOGS_PATH/sambasync.txt"
