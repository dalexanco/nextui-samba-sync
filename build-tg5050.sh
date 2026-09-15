#!/bin/sh
# Cross-compile Samba Sync in the Docker toolchain, then push the binary to a
# connected device over ADB. Run from anywhere.
export PLATFORM=tg5050
cd "$(dirname "$0")" || exit
sh run-docker.sh /bin/sh -c "cd /root/workspace/$PLATFORM/libmsettings && make build CROSS_COMPILE=aarch64-nextui-linux-gnu- PREFIX=/opt/nextui PREFIX_LOCAL=/opt/nextui && cd /root/workspace/nextui-samba-sync/lib && sh build-libsmb2.sh $PLATFORM && cd /root/workspace/nextui-samba-sync/src && make PLATFORM=$PLATFORM" || exit
adb push "bin/$PLATFORM/sambasync.elf" "/mnt/SDCARD/Tools/$PLATFORM/Samba Sync.pak/bin/$PLATFORM/sambasync.elf"
