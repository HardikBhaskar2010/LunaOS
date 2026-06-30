#!/usr/bin/env bash
# build-image.sh — Raw Disk Image Builder for Mahina Stage 0
# Authority: DL-012 (UEFI standard layout)
# Authority: DL-027 (Btrfs root filesystem)
#
# Creates a 2GB raw disk image with a FAT32 ESP and a Btrfs root.
# Installs limine bootloader.
# Copies system files into the image.

set -e

VERSION="0.1.0"
BUILD_DIR="$(pwd)/build"
IMG_SIZE="2G"
TMP_IMG="/tmp/mahina-${VERSION}.img"
IMG_FILE="${BUILD_DIR}/mahina-${VERSION}.img"

echo "  IMAGE   Building $IMG_FILE..."

# Check prerequisites
if ! command -v parted >/dev/null 2>&1; then echo "ERROR: parted required"; exit 1; fi
if ! command -v mkfs.fat >/dev/null 2>&1; then echo "ERROR: dosfstools required"; exit 1; fi
if ! command -v mkfs.btrfs >/dev/null 2>&1; then echo "ERROR: btrfs-progs required"; exit 1; fi

# Create sparse file
truncate -s $IMG_SIZE "$TMP_IMG"

# Partition table (GPT)
# 1. ESP (FAT32, 128MB)
# 2. Root (Btrfs, remaining)
parted -s "$TMP_IMG" mklabel gpt
parted -s "$TMP_IMG" mkpart ESP fat32 1MiB 129MiB
parted -s "$TMP_IMG" set 1 esp on
parted -s "$TMP_IMG" mkpart ROOT btrfs 129MiB 100%

# Setup loop devices
LOOP_DEV=$(sudo losetup --find --partscan --show "$TMP_IMG")
LOOP_ESP="${LOOP_DEV}p1"
LOOP_ROOT="${LOOP_DEV}p2"

echo "  IMAGE   Loop device: $LOOP_DEV"

# Format filesystems
sudo mkfs.fat -F32 "$LOOP_ESP" >/dev/null
sudo mkfs.btrfs -f -L MAHINA_ROOT "$LOOP_ROOT" >/dev/null

# Mount root and create subvolumes
MNT_ROOT=$(mktemp -d)
sudo mount "$LOOP_ROOT" "$MNT_ROOT"
sudo btrfs subvolume create "$MNT_ROOT/@" >/dev/null
sudo btrfs subvolume create "$MNT_ROOT/@home" >/dev/null
sudo btrfs subvolume create "$MNT_ROOT/@snapshots" >/dev/null
sudo umount "$MNT_ROOT"

# Remount with @ subvolume as default target for installation
sudo mount -o subvol=@ "$LOOP_ROOT" "$MNT_ROOT"

# Mount ESP inside root
sudo mkdir -p "$MNT_ROOT/boot/efi"
sudo mount "$LOOP_ESP" "$MNT_ROOT/boot/efi"

echo "  IMAGE   Installing files..."

# Create directory structure
sudo mkdir -p "$MNT_ROOT"/{usr/bin,usr/sbin,etc/luna/services,var/log/luna-init,tmp,run,proc,sys,dev}
# /usr merge symlinks
sudo ln -s usr/bin "$MNT_ROOT/bin"
sudo ln -s usr/sbin "$MNT_ROOT/sbin"

# Copy luna-init and luna-init-ctl
sudo cp "${BUILD_DIR}/luna-init/luna-init" "$MNT_ROOT/usr/sbin/"
sudo cp "${BUILD_DIR}/luna-init-ctl/luna-init-ctl" "$MNT_ROOT/usr/bin/"
if [ -f "${BUILD_DIR}/luna-splash/luna-splash" ]; then
    sudo cp "${BUILD_DIR}/luna-splash/luna-splash" "$MNT_ROOT/usr/sbin/"
fi

if [ -f "${BUILD_DIR}/lgp-compositor/lgp-compositor" ]; then
    sudo cp "${BUILD_DIR}/lgp-compositor/lgp-compositor" "$MNT_ROOT/usr/bin/"
fi
if [ -f "${BUILD_DIR}/lgp-compositor/lgp-test-client" ]; then
    sudo cp "${BUILD_DIR}/lgp-compositor/lgp-test-client" "$MNT_ROOT/usr/bin/"
fi
if [ -f "${BUILD_DIR}/luna-shell/luna-shell" ]; then
    sudo cp "${BUILD_DIR}/luna-shell/luna-shell" "$MNT_ROOT/usr/bin/"
fi
if [ -f "${BUILD_DIR}/luna-terminal/luna-terminal" ]; then
    sudo cp "${BUILD_DIR}/luna-terminal/luna-terminal" "$MNT_ROOT/usr/bin/"
fi

# Fetch and install busybox for an emergency shell
if [ ! -f "${BUILD_DIR}/busybox" ]; then
    echo "  IMAGE   Downloading busybox..."
    wget -qO "${BUILD_DIR}/busybox" "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox"
    chmod +x "${BUILD_DIR}/busybox"
fi
sudo cp "${BUILD_DIR}/busybox" "$MNT_ROOT/usr/bin/"
sudo ln -s /usr/bin/busybox "$MNT_ROOT/usr/bin/sh"

# Copy configs
sudo cp -r etc/luna/* "$MNT_ROOT/etc/luna/"

# Create basic system files
echo "root:x:0:0:root:/root:/bin/sh" | sudo tee "$MNT_ROOT/etc/passwd" >/dev/null
echo "root:x:0:" | sudo tee "$MNT_ROOT/etc/group" >/dev/null
echo "video:x:44:" | sudo tee -a "$MNT_ROOT/etc/group" >/dev/null

# Copy bootloader, kernel, initramfs
sudo cp boot/limine.conf "$MNT_ROOT/boot/efi/"
if [ -f "${BUILD_DIR}/vmlinuz-mahina" ]; then
    sudo cp "${BUILD_DIR}/vmlinuz-mahina" "$MNT_ROOT/boot/efi/"
else
    echo "  IMAGE   WARNING: vmlinuz-mahina not found. Kernel must be provided separately."
    # We will create a dummy kernel so Limine doesn't immediately crash before we can see its menu
    sudo touch "$MNT_ROOT/boot/efi/vmlinuz-mahina"
fi
sudo cp "${BUILD_DIR}/initramfs-mahina.img" "$MNT_ROOT/boot/efi/"

# Setup limine bootloader
# (Assumes limine binary is available on host, or downloaded. For this script,
# we fetch a prebuilt limine binary if not present)
if [ ! -f "${BUILD_DIR}/limine/limine" ]; then
    echo "  IMAGE   Downloading limine bootloader..."
    rm -rf "${BUILD_DIR}/limine"
    git clone --depth 1 -b v8.x-binary https://github.com/limine-bootloader/limine.git "${BUILD_DIR}/limine" >/dev/null 2>&1
fi

sudo mkdir -p "$MNT_ROOT/boot/efi/EFI/BOOT"
sudo cp "${BUILD_DIR}/limine/BOOTX64.EFI" "$MNT_ROOT/boot/efi/EFI/BOOT/"
sudo cp "${BUILD_DIR}/limine/BOOTIA32.EFI" "$MNT_ROOT/boot/efi/EFI/BOOT/" 2>/dev/null || true

# Unmount
sudo umount "$MNT_ROOT/boot/efi"
sudo umount "$MNT_ROOT"
rmdir "$MNT_ROOT"
sudo losetup -d "$LOOP_DEV"

cp "$TMP_IMG" "$IMG_FILE"
rm -f "$TMP_IMG"

echo "  IMAGE   Complete: $IMG_FILE"
