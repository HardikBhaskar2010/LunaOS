# LunaOS — Linux Architecture
**Volume II · Chapter 3**
**Classification:** Core Architecture — Kernel Configuration
**Status:** Active · Reference for kernel build system

---

## Purpose

This document specifies the LunaOS Linux kernel configuration: which kernel features are enabled, which are disabled, and the reasoning for each decision. It covers the kernel config philosophy, the `.config` maintenance process, and the kernel's role in the LunaOS system architecture.

---

## Overview

LunaOS uses the Linux 6.6.x LTS kernel (DL-009). The kernel configuration is not a stock distro config — it is a deliberately trimmed and tuned set of options chosen to support the LunaOS hardware target, LGP graphics stack, audio pipeline, and security posture while removing features that add complexity and attack surface without benefit.

The config file `kernel/.config` is tracked in git. Every non-default option has a corresponding comment in `kernel/.config.notes` explaining why it was set that way.

---

## Design Philosophy

The kernel configuration must satisfy Core Law I (Own Every Layer):

- Every enabled feature was explicitly evaluated.
- Every disabled feature was explicitly evaluated and rejected.
- No option exists in the config because "it was default" without acknowledgment.
- The config is never taken wholesale from another distribution.

**Key consequences of this policy:**

- The kernel is built from an unmodified upstream 6.6.x LTS tarball. No downstream patches are applied in v1.
- Every kernel config option enabled for graphics supports the LGP compositor — not Wayland, not Wayland-protocol clients. The DRM/KMS subsystem is the hardware interface layer below LGP.
- Kernel graphics options are minimal: DRM for hardware access, framebuffer for boot splash. The display protocol above that is LGP.

A trimmed kernel produces:
- Faster compilation during development
- Smaller attack surface
- Faster boot (fewer modules to initialize)
- Intentional architecture (every running component is known)

The tradeoff is reduced hardware compatibility. LunaOS v1 targets a defined hardware set. Users on unsupported hardware will encounter missing driver issues. This is documented and expected.

---

## Architecture

### Kernel Source Management

```
kernel/
├── .config              # Active kernel config (tracked in git)
├── .config.notes        # Human-readable explanations per option
├── patches/             # LunaOS-specific patches (empty at project start)
├── build.sh             # Build script: make -j$(nproc) all
└── install.sh           # Install vmlinuz + modules to /boot
```

Build command:

```sh
cd kernel/
make -j$(nproc) all
make modules_install INSTALL_MOD_PATH=/mnt/lunaos
make install INSTALL_PATH=/mnt/lunaos/boot
```

### Config Maintenance Process

1. Clone the 6.6.x LTS kernel source.
2. Copy `kernel/.config` to the source tree.
3. Run `make oldconfig` to check for new unset options.
4. Evaluate each new option — set explicitly or mark as not applicable.
5. Update `kernel/.config.notes` for every changed option.
6. Commit both files together. Never commit `.config` without `.config.notes`.

When upgrading between 6.6.x patch versions:
- Apply security patches only.
- Run `make oldconfig` after each upgrade.
- Evaluate and document any new config options before the upgrade is committed.

---

## Current Decisions

### Processor and Platform

| Option | Value | Reason |
|---|---|---|
| `CONFIG_X86_64` | `y` | x86_64 target architecture (DL-009) |
| `CONFIG_SMP` | `y` | Multi-core support — required for usable desktop |
| `CONFIG_NR_CPUS` | `16` | Reasonable ceiling; reduces per-CPU overhead vs. 512 |
| `CONFIG_PREEMPT` | `y` | Full kernel preemption — lower UI latency |
| `CONFIG_HZ_1000` | `y` | 1ms timer resolution — needed for audio and animation timing |
| `CONFIG_GENERIC_CPU` | `y` | Generic x86_64 for distribution builds — not tuned to specific microarch |

```
TODO:
Decision not yet finalized.
Reason: CONFIG_GENERIC_CPU vs. CONFIG_NATIVE trade-off not formally decided.
For distributed kernel images, GENERIC is required.
Users building from source may override with NATIVE.
This behavior should be documented in the build guide.
```

### Memory Management

| Option | Value | Reason |
|---|---|---|
| `CONFIG_TRANSPARENT_HUGEPAGE` | `y` | Large page support — reduces TLB pressure under AI workloads |
| `CONFIG_TRANSPARENT_HUGEPAGE_MADVISE` | `y` | madvise-only mode; not always-on |
| `CONFIG_ZSWAP` | `y` | Compressed swap — useful when Ollama holds model in RAM |
| `CONFIG_ZRAM` | `m` | Module; user-enableable compressed RAM block device |
| `CONFIG_MEMCG` | `y` | Memory cgroups — required for per-process memory accounting |
| `CONFIG_NUMA` | `n` | Disabled — adds complexity without benefit on target hardware |

### Scheduler

| Option | Value | Reason |
|---|---|---|
| `CONFIG_HZ_1000` | `y` | 1kHz tick for responsive UI and animation timing |
| `CONFIG_NO_HZ_FULL` | `y` | Tickless kernel for CPU-bound tasks (AI inference) |
| `CONFIG_CGROUPS` | `y` | Per-process resource limits |
| `CONFIG_SCHED_AUTOGROUP` | `y` | Automatic task grouping — desktop responsiveness during AI load |
| `CONFIG_FAIR_GROUP_SCHED` | `y` | CFS group scheduling — isolates AI daemon from interactive processes |

The scheduler configuration prioritizes interactive responsiveness. `luna-ai-d` and Ollama are background tasks that must not starve the compositor or shell. `FAIR_GROUP_SCHED` + `SCHED_AUTOGROUP` enforces this without requiring explicit cgroup configuration per process.

Full scheduler design is documented in `05_scheduler.md`.

### Filesystem Support

| Option | Value | Reason |
|---|---|---|
| `CONFIG_EXT4_FS` | `y` | Root filesystem — mature, well-tested |
| `CONFIG_BTRFS_FS` | `m` | Module; user may want Btrfs for snapshots |
| `CONFIG_TMPFS` | `y` | Required for /tmp, /run |
| `CONFIG_PROC_FS` | `y` | Required for /proc |
| `CONFIG_SYSFS` | `y` | Required for /sys |
| `CONFIG_DEVTMPFS` | `y` | Required for /dev auto-population |
| `CONFIG_INOTIFY_USER` | `y` | Required for file watching (lpkg, luna-ai-d) |
| `CONFIG_FANOTIFY` | `y` | Required for file access monitoring (LUNA.AI file observation) |
| `CONFIG_FUSE_FS` | `m` | Module; for userspace filesystems |

```
TODO:
Decision not yet finalized.
Reason: Root filesystem choice (ext4 vs. Btrfs) has not been formally recorded
in a Decision Log entry. Btrfs would provide snapshot capability useful for
system recovery and rollback. A DL entry for this decision is required before
the installer (Volume V / Chapter 6) is written.
```

### Networking

| Option | Value | Reason |
|---|---|---|
| `CONFIG_NET` | `y` | Networking subsystem |
| `CONFIG_INET` | `y` | IPv4 |
| `CONFIG_IPV6` | `y` | IPv6 — included |
| `CONFIG_NETFILTER` | `y` | Required for firewall (nftables) |
| `CONFIG_NF_TABLES` | `y` | nftables — modern firewall, replaces iptables |
| `CONFIG_PACKET` | `y` | Raw packet sockets — required by NetworkManager |
| `CONFIG_UNIX` | `y` | Unix domain sockets — required by D-Bus, PipeWire, IPC |
| `CONFIG_WIRELESS` | `y` | Wi-Fi support |
| `CONFIG_MAC80211` | `y` | Wi-Fi stack |
| `CONFIG_CFG80211` | `y` | Wireless configuration API |

Full networking architecture is in `10_networking.md`.

### Graphics and Display

The kernel graphics layer provides hardware access to the LGP compositor. The kernel is not aware of LGP. It provides DRM/KMS primitives: mode setting, buffer management, GPU command submission. LGP operates above these primitives in userspace.

Wayland is not referenced at the kernel level. The DRM/KMS subsystem is protocol-agnostic and supports any display server that uses it correctly.

| Option | Value | Reason |
|---|---|---|
| `CONFIG_DRM` | `y` | Direct Rendering Manager — hardware access for LGP compositor |
| `CONFIG_DRM_KMS_HELPER` | `y` | Kernel mode-setting helper |
| `CONFIG_DRM_I915` | `m` | Intel integrated GPU — module |
| `CONFIG_DRM_AMDGPU` | `m` | AMD GPU — module |
| `CONFIG_DRM_NOUVEAU` | `m` | Nvidia open-source — module |
| `CONFIG_FRAMEBUFFER_CONSOLE` | `y` | Required for boot splash framebuffer renderer |
| `CONFIG_FB` | `y` | Framebuffer subsystem — boot splash |
| `CONFIG_DRM_PANEL_ORIENTATION_QUIRKS` | `y` | Laptop display orientation |

```
TODO:
Decision not yet finalized.
Reason: GPU backend for the LGP compositor (Vulkan, OpenGL, DRM direct) has not
been decided. The kernel config above enables DRM for all three paths.
Once Volume III / 01_lgp.md specifies the LGP rendering backend, the kernel
config may need to be updated (e.g., enabling Vulkan-specific DRM features).
```

Proprietary Nvidia drivers (`nvidia.ko`) are not included in the kernel config. Users requiring proprietary drivers install them via `lpkg` after installation.

```
TODO:
Decision not yet finalized.
Reason: Proprietary GPU driver distribution strategy has not been decided.
The legal and repository implications of distributing proprietary modules have
not been addressed. This requires a Decision Log entry.
```

### Audio

| Option | Value | Reason |
|---|---|---|
| `CONFIG_SOUND` | `y` | Sound subsystem |
| `CONFIG_SND` | `y` | ALSA core |
| `CONFIG_SND_HDA_INTEL` | `m` | Intel HDA — most common desktop/laptop audio |
| `CONFIG_SND_USB_AUDIO` | `m` | USB audio devices |
| `CONFIG_SND_ALOOP` | `m` | Loopback device — useful for audio routing with PipeWire |

PipeWire sits above ALSA in userspace and handles all audio routing. The kernel audio config provides the hardware interface only.

### Security

| Option | Value | Reason |
|---|---|---|
| `CONFIG_SECURITY` | `y` | Linux Security Module framework |
| `CONFIG_SECURITY_SELINUX` | `n` | Not included — complexity without corresponding benefit |
| `CONFIG_SECURITY_APPARMOR` | `y` | AppArmor for application sandboxing |
| `CONFIG_DEFAULT_SECURITY_APPARMOR` | `y` | AppArmor is the default LSM |
| `CONFIG_SECCOMP` | `y` | Sandboxed process filtering |
| `CONFIG_SECCOMP_FILTER` | `y` | BPF-based seccomp rules |
| `CONFIG_NAMESPACES` | `y` | Namespace isolation |
| `CONFIG_USER_NS` | `y` | User namespaces — rootless application isolation |
| `CONFIG_RANDOMIZE_BASE` | `y` | KASLR — kernel address space randomization |
| `CONFIG_STRICT_KERNEL_RWX` | `y` | Non-executable kernel data pages |

Full security architecture is documented in `08_security.md`.

### Virtualization Support

| Option | Value | Reason |
|---|---|---|
| `CONFIG_VIRTIO` | `y` | VirtIO for running LunaOS in VMs during development |
| `CONFIG_VIRTIO_PCI` | `y` | VirtIO PCI transport |
| `CONFIG_VIRTIO_NET` | `y` | VirtIO network |
| `CONFIG_VIRTIO_BLOCK` | `y` | VirtIO block |
| `CONFIG_KVM_GUEST` | `y` | KVM guest support for development VMs |

VirtIO options are included because LunaOS development is done in virtual machines before bare-metal testing. They do not add meaningful overhead on physical hardware and do not need to be disabled for release.

### Power Management

| Option | Value | Reason |
|---|---|---|
| `CONFIG_PM` | `y` | Power management framework |
| `CONFIG_PM_SLEEP` | `y` | Suspend/resume support |
| `CONFIG_CPU_FREQ` | `y` | CPU frequency scaling |
| `CONFIG_CPU_FREQ_GOVERNOR_SCHEDUTIL` | `y` | schedutil — integrates with scheduler |
| `CONFIG_CPU_IDLE` | `y` | CPU idle states |
| `CONFIG_ACPI` | `y` | ACPI — required for modern hardware |

---

## Technical Details

### Build System Integration

```
build-lunaos.sh
    │
    ├── [1] Build luna-init (C compiler)
    ├── [2] Build kernel (make -j<N>)
    ├── [3] Build lpkg (Python packaging)
    ├── [4] Build LGP compositor and shell
    ├── [5] Build other userland components
    └── [6] Assemble ISO (build-iso.sh → limine)
```

Kernel build is the longest single step. Reference build time on 4-core hardware: approximately 20–40 minutes.

### Module Loading

Kernel modules are loaded by luna-init at Stage 3 boot from `/etc/luna/modules.conf`:

```toml
# /etc/luna/modules.conf
# Hardware-specific modules loaded at boot
modules = [
    "drm_kms_helper",
    "i915",          # Intel GPU — remove if AMD/Nvidia
    "snd_hda_intel",
    "iwlwifi",       # Intel Wi-Fi — hardware specific
]
```

This file is generated during installation based on hardware detection. It is user-editable. No modules are loaded automatically beyond this list.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| ARM64 config | Post v1.0 | Requires parallel `.config.arm64` and cross-compilation pipeline |
| PREEMPT_RT patch | v1.5 | Real-time preemption for audio latency improvement |
| musl-compatible kernel options | v2.0 | Audit required at musl migration |
| Full security audit | v1.0 | Review all security config options before public release |
| Hardware compatibility matrix | v1.0 | Define and publish tested hardware list |
| LGP-specific kernel options | v1.0 | After Volume III / 01_lgp.md defines the LGP GPU backend |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Root filesystem ext4 vs. Btrfs.** No Decision Log entry exists. Must be resolved before installer work begins.

2. **Proprietary GPU driver strategy.** Distribution and legal implications not yet addressed. Must be a Decision Log entry.

3. **LGP GPU backend kernel requirements.** Depends on Volume III / 01_lgp.md. Kernel config may need updates once LGP backend is specified.

4. **CONFIG_GENERIC_CPU vs. CONFIG_NATIVE.** Must be documented in the build guide before v1 release.

5. **PREEMPT_RT for v1.** `CONFIG_PREEMPT` (full preemption) is chosen. `PREEMPT_RT` (real-time) provides lower audio latency but requires applying the RT patch series. Decision deferred to v1 audio testing.

6. **Hardware support matrix.** Before v1.0 public release, a tested hardware list must be published and the kernel config verified against it.

---

## AI Context

An AI agent building LunaOS or modifying the kernel configuration must understand:

- The kernel config is in `kernel/.config` and is version-controlled. Do not use a stock distro `.config`.
- Every option has a note in `kernel/.config.notes`. Adding or changing an option requires adding or updating its note in the same commit.
- `CONFIG_PREEMPT=y` (full preemption) is intentional. Do not change to `VOLUNTARY`.
- `CONFIG_HZ_1000=y` is required for 1ms timer resolution — affects animation timing accuracy. Do not reduce.
- `CONFIG_DRM=y` provides hardware access for the **LGP compositor**. It is not a Wayland kernel option. DRM/KMS is protocol-agnostic.
- VirtIO options are present for development VM support, not production dependency.
- AppArmor is the default LSM. SELinux is not present.
- All hardware-specific modules are compiled as `m` (modules), not `y` (built-in). This is intentional.
- The LGP graphics protocol operates in userspace above DRM/KMS. No kernel-level LGP changes are anticipated. If a kernel change is being considered for graphics, verify against Volume III / 01_lgp.md first.

---

*Document: `Volume II / 03_linux_architecture.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-draft*
*Depends on: architecture_overview.md, decision_log.md (DL-001, DL-007, DL-009), core_laws.md, non_negotiables.md*
*Supersedes: v0.1-draft (referenced Wayland as graphics target — non-compliant)*
