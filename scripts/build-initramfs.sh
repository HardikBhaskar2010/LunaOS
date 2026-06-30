#!/usr/bin/env bash
# build-initramfs.sh — Custom Initramfs Generator for Mahina Stage 0
# Authority: Volume II / 02_boot_flow.md (Initramfs Tooling open question resolved)
#
# Builds an initramfs containing:
#   - statically linked luna-init
#   - busybox (static) for emergency shell
#   - minimal device nodes

set -e

VERSION="0.1.0"
BUILD_DIR="$(pwd)/build"
INITRAMFS_DIR="/tmp/initramfs-mahina"
OUT_IMG="${BUILD_DIR}/initramfs-mahina.img"

echo "  INITRAMFS Creating directory structure..."
if [ -e "${INITRAMFS_DIR}" ]; then
    rm -rf "${INITRAMFS_DIR}" 2>/dev/null || sudo rm -rf "${INITRAMFS_DIR}"
fi
mkdir -p "${INITRAMFS_DIR}"/{bin,sbin,etc,proc,sys,dev,tmp,run,var/log,mnt/root}

echo "  INITRAMFS Installing binaries..."
# 1. luna-init (PID 1)
cp "${BUILD_DIR}/luna-init/luna-init" "${INITRAMFS_DIR}/sbin/luna-init"
chmod +x "${INITRAMFS_DIR}/sbin/luna-init"

# 2. busybox (Emergency shell / utilities)
# Assumes busybox-static is installed on the build host
if ! command -v busybox >/dev/null 2>&1; then
    echo "ERROR: busybox not found on host. Please install busybox-static."
    exit 1
fi
cp "$(which busybox)" "${INITRAMFS_DIR}/bin/busybox"
chmod +x "${INITRAMFS_DIR}/bin/busybox"

# Create symlinks for common busybox tools
for cmd in sh ls mount umount cat echo mkdir rm cp mv dmesg modprobe; do
    ln -s /bin/busybox "${INITRAMFS_DIR}/bin/${cmd}"
done

echo "  INITRAMFS Installing additional userland binaries..."
mkdir -p "${INITRAMFS_DIR}/usr/bin"
mkdir -p "${INITRAMFS_DIR}/etc/luna/services"

# List of binaries to copy
binaries=(
    "lgp-compositor/lgp-compositor"
    "luna-shell/luna-shell"
    "luna-desktop/luna-desktop"
    "luna-terminal/luna-terminal"
    "luna-settings/luna-settings"
    "luna-files/luna-files"
    "luna-calc/luna-calc"
    "luna-installer/luna-installer"
    "luna-tasks/luna-tasks"
    "luna-about/luna-about"
    "luna-text/luna-text"
    "luna-init-ctl/luna-init-ctl"
    "luna-splash/luna-splash"
)

for b in "${binaries[@]}"; do
    name=$(basename "$b")
    src="${BUILD_DIR}/${b}"
    if [ -f "${src}" ]; then
        cp "${src}" "${INITRAMFS_DIR}/usr/bin/"
        chmod +x "${INITRAMFS_DIR}/usr/bin/${name}"
    else
        echo "WARNING: Binary ${name} not found at ${src}. Skipping."
    fi
done

# Copy service descriptors
if [ -d "$(pwd)/etc/luna/services" ]; then
    cp -r "$(pwd)/etc/luna/services/"* "${INITRAMFS_DIR}/etc/luna/services/"
else
    echo "WARNING: etc/luna/services not found. Skipping."
fi

# Create basic passwd/group files inside initramfs
mkdir -p "${INITRAMFS_DIR}/etc"
echo "root:x:0:0:root:/root:/bin/sh" > "${INITRAMFS_DIR}/etc/passwd"
echo "root:x:0:" > "${INITRAMFS_DIR}/etc/group"
echo "video:x:44:" >> "${INITRAMFS_DIR}/etc/group"

echo "  INITRAMFS Installing kernel modules..."
mkdir -p "${INITRAMFS_DIR}/lib/modules"
if [ -d "${BUILD_DIR}/kernel/lib/modules" ]; then
    cp -r "${BUILD_DIR}/kernel/lib/modules/"* "${INITRAMFS_DIR}/lib/modules/"
fi

echo "  INITRAMFS Creating early /dev nodes..."
# These are needed before devtmpfs is mounted
MKNOD="mknod"
if [ "$(id -u)" -ne 0 ]; then
    MKNOD="sudo mknod"
fi
${MKNOD} -m 600 "${INITRAMFS_DIR}/dev/console" c 5 1
${MKNOD} -m 666 "${INITRAMFS_DIR}/dev/null" c 1 3
${MKNOD} -m 666 "${INITRAMFS_DIR}/dev/zero" c 1 5

echo "  INITRAMFS Writing init script..."
# The init script run by the kernel
cat << 'EOF' > "${INITRAMFS_DIR}/init"
#!/bin/sh
# Mahina Early Initramfs Script

export PATH=/sbin:/bin

echo "==================================================="
echo "[initramfs] Early Initramfs Script Started!!!"
echo "==================================================="

# Mount essential pseudo-filesystems
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev

echo "[initramfs] Loading kernel modules..."
modprobe virtio_blk || echo "Warning: virtio_blk not loaded"
modprobe btrfs || echo "Warning: btrfs not loaded"
sleep 1 # Give devtmpfs a moment to create the /dev/vda2 block device

# Mount the real root filesystem (virtio-blk /dev/vda2 for QEMU)
# In a real setup, this would parse root= from /proc/cmdline
echo "[initramfs] Mounting real root: /dev/vda2"
mount -t btrfs -o rw,subvol=@ /dev/vda2 /mnt/root || {
    echo "[initramfs] FATAL: Failed to mount real root"
    echo "Dropping to emergency shell..."
    exec busybox setsid busybox cttyhack sh
}

# Clean up pseudo-filesystems from initramfs
umount /dev
umount /sys
umount /proc

# Pivot root and exec luna-init
echo "[initramfs] Pivoting root..."
exec busybox switch_root /mnt/root /sbin/luna-init
EOF

chmod +x "${INITRAMFS_DIR}/init"

echo "  INITRAMFS Packing image..."
cd "${INITRAMFS_DIR}"
find . | cpio -H newc -o --quiet | gzip -9 > "${OUT_IMG}"
cd - >/dev/null

echo "  INITRAMFS Done: ${OUT_IMG}"
