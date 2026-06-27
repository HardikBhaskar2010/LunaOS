# Mahina — Boot Flow
**Volume II · Chapter 2**
**Classification:** Core Architecture — Boot Sequence
**Status:** Active · Reference for luna-init implementation

---

## Purpose

This document specifies the complete Mahina boot sequence from UEFI firmware handoff to a fully operational desktop with LUNA.AI online. It defines stage boundaries, success criteria per stage, timing targets, error handling behavior, and the user-visible boot experience.

This document is the authoritative reference for:
- `luna-init` boot stage implementation
- Boot splash and animation timing
- Error recovery design during early boot
- Future automated boot testing

---

## Overview

Mahina boots in seven stages. Each stage is a discrete, verifiable state. Failure in any stage produces a defined error behavior rather than an unhandled crash. The user-visible boot animation is synchronized to stage transitions per the Motion Vocabulary (`core_laws.md` Law III).

**Total target boot time to desktop: under 8 seconds on reference hardware.**

```
TODO:
Decision not yet finalized.
Reason: Reference hardware has not been formally specified.
All timing targets in this document are provisional until a minimum hardware spec
is formally recorded in a Decision Log entry.
See: architecture_overview.md Open Question 4.
```

---

## Design Philosophy

The boot sequence follows three principles derived from the Core Laws:

**1. Visibility (Law III).** The user must always know what stage the system is in. Each stage has a corresponding visual indicator from the locked Motion Vocabulary. A black screen with no status is never acceptable.

**2. Forward progress (Law V).** Stages do not retry indefinitely. A service that fails to start after the configured attempt limit enters a degraded state and boot continues where possible. A system that hangs is worse than one that boots to a reduced desktop.

**3. The boot animation communicates state (Law III).** Each animation state maps to a real system state (scanline sweep = initialization, particle burst = stage complete). This is Law III at the boot level. No animation type may be used that is not in the locked Motion Vocabulary.

---

## Architecture

### Stage Map

```
Power on
    │
    ▼
[Stage 0] UEFI → Bootloader
    │  limine presents boot menu (1s timeout)
    │  Kernel image + initramfs loaded
    ↓
[Stage 1] Kernel Init
    │  Hardware detection, driver init
    │  PID 1 handed to luna-init
    ↓
[Stage 2] Filesystem Mount
    │  Root filesystem mounted
    │  /proc, /sys, /dev, /tmp mounted
    │  Swap activated if configured
    ↓
[Stage 3] Early System Hooks
    │  Hostname set from /etc/luna/hostname
    │  System clock synced (hwclock)
    │  Entropy seeded
    │  Kernel modules loaded
    ↓
[Stage 4] System Services
    │  Firewall (nftables) starts first
    │  D-Bus daemon starts
    │  NetworkManager starts
    │  PipeWire starts
    │  NTP service starts
    │  (Parallel where dependencies allow)
    ↓
[Stage 5] Graphics Layer
    │  GPU/framebuffer readiness check
    │  luna-splash receives SIGTERM
    │  LGP compositor starts
    ↓
[Stage 6] Shell + LUNA Presence Engine
    │  luna-shell starts
    │  luna-bar starts
    │  luna-island starts
    │  luna-notif starts
    │  luna-ai-d (Presence Engine) starts
    ↓
[Stage 7] Desktop Ready
       LUNA Presence Engine online signal sent to luna-island
       LLM Inference Engine waits for first demand
       Session ready
```

### Stage Boundaries and Success Criteria

| Stage | Entry Condition | Success Criterion | Failure Behavior |
|---|---|---|---|
| 0 | UEFI boot | Kernel loaded, PID 1 alive | Hardware failure — cannot recover |
| 1 | PID 1 = luna-init | Filesystems writable | Drop to emergency shell |
| 2 | Filesystems mounted | /proc, /sys, /dev visible | Drop to emergency shell |
| 3 | Early hooks | Hostname set, clock valid | Warn and continue (non-fatal) |
| 4 | Firewall + D-Bus alive | All Stage 4 services running | Mark degraded, continue |
| 5 | GPU/framebuffer ready | LGP compositor accepting connections | Fallback to framebuffer console |
| 6 | Compositor alive | Presence Engine online | Degraded desktop (no LUNA) |
| 7 | Shell alive | luna-island shows LUNA Presence online | LLM deferred to first demand |

**Emergency shell:** A minimal statically-linked shell on TTY1. Available when Stage 1 or Stage 2 fails. Provides `lpkg`, `luna-init` control commands, and filesystem repair tools.

**Degraded desktop:** A functional compositor + shell without LUNA.AI. All user applications work. LUNA.AI features are unavailable. luna-island shows a warning indicator using the LUNA Amber color (`#FFB800`) from the Color Semantic Contract.

---

## Technical Details

### Stage 0 — UEFI to limine

limine is configured in `/boot/limine.cfg`:

```toml
# /boot/limine.cfg
TIMEOUT=1

:Mahina
PROTOCOL=linux
KERNEL_PATH=boot:///vmlinuz-mahina
CMDLINE=root=/dev/sda1 rw quiet splash
MODULE_PATH=boot:///initramfs-mahina.img
```

Boot parameters:
- `quiet` — suppress kernel printk output to console during normal boot
- `splash` — luna-init activates boot splash mode (framebuffer renderer)
- `rw` — root filesystem mounted read-write immediately

```
TODO:
Decision not yet finalized.
Reason: A debug boot parameter for development use has not been specified.
Consideration: A parameter such as "lunadebug" that disables quiet+splash,
enables verbose luna-init output, and drops to emergency shell on any stage
failure would significantly improve the development workflow.
```

### Stage 1 — Kernel Initialization

The kernel initializes hardware, loads drivers from the initramfs, and hands control to `/sbin/luna-init` (PID 1).

The initramfs contains:
- `luna-init` binary (statically linked)
- Minimal filesystem tools
- Root filesystem driver modules
- Cryptographic modules if disk encryption is enabled

After the root filesystem is mounted, the initramfs transitions to the real filesystem and re-executes `luna-init` as the permanent PID 1.

```
TODO:
Decision not yet finalized.
Reason: Initramfs build tooling has not been decided.
Options: dracut, mkinitcpio, or a custom initramfs generator.
Using an existing tool conflicts with Core Law I in spirit.
Writing a custom one is significant additional scope.
This must be resolved before boot system implementation begins.
```

### Stage 2 — Filesystem Mount

`luna-init` reads `/etc/luna/fstab.toml` (TOML format):

```toml
# /etc/luna/fstab.toml
[[mount]]
device = "/dev/sda1"
mountpoint = "/"
fstype = "btrfs"
options = ["rw", "relatime"]

[[mount]]
device = "tmpfs"
mountpoint = "/tmp"
fstype = "tmpfs"
options = ["size=2G", "noexec", "nosuid"]

[[mount]]
device = "proc"
mountpoint = "/proc"
fstype = "proc"
options = []

[[mount]]
device = "sysfs"
mountpoint = "/sys"
fstype = "sysfs"
options = []

[[mount]]
device = "devtmpfs"
mountpoint = "/dev"
fstype = "devtmpfs"
options = []
```

Mounts are attempted in declaration order. Failure to mount `/proc`, `/sys`, or `/dev` is fatal (drop to emergency shell). Failure to mount user-defined additional mounts produces a warning and continues.

### Stage 3 — Early Hooks

Executed sequentially within luna-init:

1. **Hostname:** `sethostname()` syscall using value from `/etc/luna/hostname`.
2. **Clock:** `hwclock --hctosys --utc` to synchronize hardware clock.
3. **Entropy:** Write to kernel entropy pool from available hardware sources.
4. **Boot Splash:** Spawn `luna-splash` to render the boot animation.
5. **Kernel modules:** Load each module listed in `/etc/luna/modules.conf` via `modprobe`. Failures are logged, do not halt boot.
6. **sysctl parameters:** Apply settings from `/etc/luna/sysctl.toml`.

All Stage 3 operations are non-fatal. Failures write to `/var/log/luna-init/boot.log`.

### Stage 4 — System Services

`luna-init` reads service files from `/etc/luna/services/`. Each service declares dependencies. `luna-init` builds a dependency graph and starts services in dependency order, parallelizing where the graph permits.

Stage 4 reference service files:

```
/etc/luna/services/nftables.toml     # Firewall — starts first, no dependencies
/etc/luna/services/dbus.toml
/etc/luna/services/networkmanager.toml
/etc/luna/services/pipewire.toml
/etc/luna/services/wireplumber.toml
/etc/luna/services/ntpd.toml         # Time sync — DL-015
```

Note: **Ollama is not a Stage 4 service.** Per DL-021, Ollama is started lazily by the LLM Inference Engine on first user demand. It is not in the boot-time service set.

Service file format is documented in `04_init_system.md`.

A service that fails after the configured restart attempt limit is marked `DEGRADED`. `luna-init` logs the failure and continues. The desktop session starts in a degraded state with a warning in luna-island.

**Timing target for Stage 4:** Under 2 seconds from D-Bus start to all services healthy.

### Stage 5 — Graphics Layer

**Boot splash visual states (Motion Vocabulary — `core_laws.md` Law III):**

```
Stages 0-3:   Scanline sweep — system initializing
Stage 4:      Slow pulse — services loading
Stage 5:      Compositor starts, splash fades → compositor takes over
Stage 6:      Compositor active — no motion in compositor itself during shell load
Stage 7:      Particle burst — boot complete
```

All motion types above are from the locked Motion Vocabulary. No other motion types may be used during boot.

**Framebuffer-to-compositor handoff:**

Per DL-043, Mahina accepts a brief visual transition (single black frame, ~16ms at 60Hz) between the `luna-splash` framebuffer boot splash and the LGP compositor's first frame.
- `luna-init` spawns `luna-splash` at Stage 3.
- `luna-init` sends SIGTERM to `luna-splash` before the compositor takes over in Stage 5.
- The lgp-compositor's first frame is its own rendered output.
- No architectural complexity is introduced to eliminate this cut.

### Stage 6 — Shell and LUNA Presence Engine

Per **DL-021**, LUNA consists of two independent systems. Only the **Presence Engine** starts at boot. The **LLM Inference Engine** (Ollama + model weights) is lazy-loaded on first demand.

After confirming the LGP compositor is ready (method TBD — see Stage 5 TODO), `luna-init` starts components in this order:

1. `luna-shell` — root desktop surface
2. `luna-bar` — status bar (depends on luna-shell)
3. `luna-island` — LUNA presence widget (depends on luna-shell)
4. `luna-notif` — notification daemon
5. `luna-ai-d` — LUNA Presence Engine **only** (no Ollama dependency at boot)

**Presence Engine start sequence:**
1. luna-ai-d starts, reads `~/.luna/config/observe.toml` (observation allow-list)
2. Presence Engine loads: personality engine, context engine, memory engine, permission engine
3. Memory store loaded from `~/.luna/memory/` (the persistent summarized record from last shutdown)
4. luna-ai-d signals readiness to luna-island — **Presence Engine online**
5. Boot is Stage 7 complete

**LLM Inference Engine — lazy initialization (NOT at boot):**

The first time any of the following occurs after boot, the LLM Inference Engine initializes:
- User starts a conversation with LUNA
- Voice interaction begins
- AI automation is requested
- Explicit reasoning is required

On first LLM demand:
1. luna-ai-d spawns Ollama (or sends start signal to luna-init to start the Ollama service)
2. Ollama starts and loads the default model into RAM (2–3 GB)
3. luna-ai-d establishes connection to localhost:11434
4. If Ollama fails to start within 10 seconds: LLM features unavailable, Presence Engine continues
5. LLM query is processed; subsequent queries use the already-loaded model

**Effect on boot RAM usage:** At Stage 7 complete, Ollama model weights are **not** in RAM. The Presence Engine footprint target is under 100 MB. The full ~3 GB Ollama RAM cost is deferred until first conversation.

```
TODO:
Decision not yet finalized.
Reason: The IPC protocol from luna-ai-d to luna-island for the Presence Engine
online signal is not yet specified. This must be decided as part of Volume IV
architecture work.
```

```
TODO:
Decision not yet finalized.
Reason: The mechanism by which luna-ai-d starts Ollama on first LLM demand
has two options:
  Option A: luna-ai-d calls luna-init-ctl to start the ollama service dynamically.
  Option B: luna-ai-d directly fork+exec Ollama (outside luna-init supervision).
Option A is preferred (all processes remain in the luna-init process tree).
This must be a Decision Log entry before Volume IV implementation begins.
```

### Stage 7 — Desktop Ready

When `luna-island` receives the LUNA readiness signal from `luna-ai-d`:
- luna-island transitions from startup state to Idle state (eyes visible, slow pulse — per Motion Vocabulary)
- Boot sequence is considered complete
- `/var/run/luna-boot-complete` sentinel file is created by luna-init

`luna-init` writes a final boot log entry:

```
[luna-init] Boot complete. Stages: 0-7. Time: X.XXs. Status: NOMINAL | DEGRADED
```

---

## Boot Timing Budget

| Stage | Target duration | Failure timeout |
|---|---|---|
| Stage 0 (UEFI → kernel) | < 1.0s | N/A (hardware) |
| Stage 1 (kernel init) | < 1.5s | N/A (hardware) |
| Stage 2 (filesystem mount) | < 0.5s | 10s per mount |
| Stage 3 (early hooks) | < 0.2s | N/A (non-fatal) |
| Stage 4 (services) | < 2.0s | 5s per service |
| Stage 5 (compositor) | < 1.5s | 10s for GPU ready |
| Stage 6 (shell + Presence Engine) | < 1.0s | 5s for Presence Engine |
| Stage 7 (desktop ready) | < 0.3s | N/A |
| **Total** | **< 8.0s** | |

Note: Stage 6 target is reduced from 1.5s to 1.0s because Ollama (the previously boot-blocking service) is no longer part of the boot sequence (DL-021).

---

## Boot Log Format

All boot events are written to `/var/log/luna-init/boot.log` in structured format:

```
[TIMESTAMP_MS] [STAGE] [COMPONENT] [LEVEL] message
```

Example:

```
[0000] [0] [luna-init]           [INFO] PID 1 alive. Mahina Waxing 0.1.0
[0120] [1] [luna-init]           [INFO] Root filesystem mounted
[0145] [2] [luna-init]           [INFO] /proc /sys /dev mounted
[0340] [3] [luna-init]           [INFO] Hostname: lunabox. Clock synced.
[0410] [4] [nftables]            [INFO] Firewall active
[0800] [4] [dbus]                [INFO] D-Bus daemon ready
[1350] [4] [pipewire]            [INFO] PipeWire session ready
[1380] [4] [ntpd]                [INFO] NTP synchronized
[2900] [5] [lgp-compositor]      [INFO] Compositor started, display server ready
[3200] [6] [luna-shell]          [INFO] Desktop surface created
[3450] [6] [luna-island]         [INFO] Luna Island initialized
[3700] [6] [luna-ai-d]           [INFO] Presence Engine online. Mode: AMBIENT
[3720] [7] [luna-init]           [INFO] Boot complete. Time: 3.72s. Status: NOMINAL
[---- ] [*] [luna-ai-d]          [INFO] (LLM Inference Engine: idle — awaiting first demand)
```

---

## Error Recovery Reference

| Failure condition | Recovery behavior | User-visible signal |
|---|---|---|
| Stage 1 failure (filesystem) | Emergency shell on TTY1 | Black screen → TTY1 text |
| Stage 4 service failure | Degraded mode, boot continues | luna-island LUNA Amber warning |
| Stage 5 compositor timeout | Fallback framebuffer console | Splash screen → TTY error |
| Stage 6 Presence Engine failure | Desktop functional, LUNA unavailable | luna-island LUNA Amber |
| Post-boot Ollama failure (on LLM demand) | LLM features unavailable, Presence continues | luna-island shows LLM disconnected state |
| Post-boot service crash | luna-init supervises and restarts per service restart policy | luna-island notification |

Colors used above (LUNA Amber `#FFB800`) are from the locked Color Semantic Contract (`core_laws.md` Law III). No other colors may be used for warning states.

---

## Future Improvements

| Improvement | Priority | Notes |
|---|---|---|
| Boot performance profiling tool | v1 | `luna-bootprof` command to analyze boot log per-stage |
| Hibernate / resume support | v1.5 | Requires kernel hibernation config + luna-init resume hook |
| Parallel Stage 4 with dependency graph | v1 | Full dependency-aware parallelism |
| Encrypted root filesystem | v1 | Requires initramfs cryptographic support |
| Secure Boot | v2 | limine signing infrastructure not yet scoped |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Boot splash implementation.** Should the boot splash be a framebuffer rendering loop inside luna-init, or a separate lightweight process spawned by luna-init? A separate process reduces PID 1 complexity but requires process management before services are online.

2. **Framebuffer-to-LGP compositor handoff.** Transition technique is unresolved. See Stage 5 Technical Details above.

3. **Initramfs tooling.** dracut, mkinitcpio, or custom generator? Must be resolved before boot system implementation.

4. **Emergency shell binary.** busybox, dash, or purpose-built? Must be statically linked and fit in the initramfs.

5. **LGP compositor process name.** Not yet assigned. All references in this document use `lgp-compositor` as a placeholder.

6. **luna-ai-d → luna-island IPC.** The exact protocol for the readiness signal is not yet specified. Depends on Volume IV architecture decisions.

---

## AI Context

An AI agent implementing the boot sequence must understand:

- `luna-init` is PID 1 and the only process allowed to spawn services. No service forks itself into a daemon — all are managed by luna-init with TOML service files.
- The graphics layer (Stage 5) starts the **LGP compositor** — not Wayland, not Hyprland, not any upstream compositor.
- The framebuffer boot splash is a rendering loop inside luna-init (current direction). It terminates when the LGP compositor takes over the display.
- All boot log timestamps are milliseconds from PID 1 start.
- `/var/run/luna-boot-complete` is created by luna-init at Stage 7 completion.
- **Ollama does NOT start at boot (DL-021).** The Presence Engine starts at boot. Ollama starts on first LLM demand. Do not add Ollama to the Stage 4 or Stage 6 service list.
- **The boot-complete state is Presence Engine online, not LLM online.** luna-island shows the LUNA Presence indicator at Stage 7 — the LLM is not yet loaded.
- All boot animation states use the locked Motion Vocabulary from `core_laws.md` Law III. Do not introduce new motion types at boot.
- All warning colors use LUNA Amber (`#FFB800`) from the locked Color Semantic Contract. Error states use LUNA Pink (`#FF2D78`). Do not use other colors for system state signals.
- NTP is a Stage 4 service (DL-015). It starts alongside NetworkManager, after D-Bus.
- Firewall (nftables) starts first in Stage 4, before NetworkManager.

---

*Document: `Volume II / 02_boot_flow.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.3-draft*
*Depends on: architecture_overview.md, core_laws.md, decision_log.md (DL-002, DL-005, DL-009, DL-015, DL-021), non_negotiables.md*
*Supersedes: v0.2-draft (Ollama incorrectly placed in boot sequence — DL-021 non-compliant)*
