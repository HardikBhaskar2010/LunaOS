#!/usr/bin/env bash
set -e

mkdir -p /tmp/mahina_mnt
LOOP_DEV=$(losetup --find --partscan --show build/mahina-0.1.0.img)
mount -o subvol=@ "${LOOP_DEV}p2" /tmp/mahina_mnt

cp /tmp/mahina_mnt/var/log/luna-init/runtime.log build/guest_runtime.log
echo "Copied log file to build/guest_runtime.log"

umount /tmp/mahina_mnt
losetup -d "$LOOP_DEV"
