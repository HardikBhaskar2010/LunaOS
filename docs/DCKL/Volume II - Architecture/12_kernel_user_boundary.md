# Mahina — Kernel/User Boundary
**Volume II · Chapter 12**
**Classification:** Core Architecture — Boundary Authority
**Status:** Canonical · Every component's responsibility ends at the boundary defined here

---

## Purpose

This document defines the **exact boundary** between every layer of Mahina — from hardware firmware to user applications. It specifies what each layer is responsible for, what it is explicitly **not** responsible for, and which interface it exposes to the layer above it.

This is the document that prevents layer violations: a userspace daemon reading kernel memory directly, a graphics library calling DRM/KMS, a compositor doing network I/O.

This document is the authoritative reference for:
- The five layers of Mahina and their exact responsibilities
- The interface contracts between each adjacent pair of layers
- What each layer is forbidden from doing
- How responsibility transfers when a component crosses layers
- State machine for system call flow

---

## Overview

Mahina has five layers. Each layer exposes exactly one interface to the layer above it and consumes exactly one interface from the layer below it.

```
┌──────────────────────────────────────────────────────────────────┐
│  Layer 5 — APPLICATIONS                                           │
│  What: Any software a user chooses to run                         │
│  Interface DOWN: LunaGUI API, LGP direct socket, POSIX stdio     │
│  Interface UP: none (top layer)                                   │
└────────────────────────────┬─────────────────────────────────────┘
                             │ LunaGUI API calls / LGP socket
┌────────────────────────────▼─────────────────────────────────────┐
│  Layer 4 — USERLAND                                               │
│  What: LunaGUI toolkit, luna-shell, luna-bar, luna-notif,         │
│        lpkg, settings, built-in applications                      │
│  Interface DOWN: LGP protocol (compositor socket)                 │
│  Interface UP: LunaGUI API (widget tree, canvas, animation)       │
└────────────────────────────┬─────────────────────────────────────┘
                             │ LGP protocol messages
┌────────────────────────────▼─────────────────────────────────────┐
│  Layer 3 — GRAPHICS                                               │
│  What: lgp-compositor, lgp-render, animation engine, theme engine │
│  Interface DOWN: Linux DRM/KMS (/dev/dri/card0), libinput        │
│  Interface UP: LGP Unix socket (/run/lgp/compositor.sock)        │
└────────────────────────────┬─────────────────────────────────────┘
                             │ DRM/KMS ioctls, libinput events
┌────────────────────────────▼─────────────────────────────────────┐
│  Layer 2 — SYSTEM SERVICES                                        │
│  What: luna-init, D-Bus, NetworkManager, PipeWire, ntpd,         │
│        luna-ai-d, nftables, lpkg (daemon component)               │
│  Interface DOWN: Linux syscall interface (POSIX)                  │
│  Interface UP: D-Bus API, /run/ socket files, POSIX files        │
└────────────────────────────┬─────────────────────────────────────┘
                             │ POSIX syscalls (open, read, write, mmap, socket...)
┌────────────────────────────▼─────────────────────────────────────┐
│  Layer 1 — LINUX KERNEL                                           │
│  What: Process management, memory management, filesystem,         │
│        device drivers, network stack, security (AppArmor)         │
│  Interface DOWN: Hardware (CPU, GPU, NIC, storage, input devices) │
│  Interface UP: POSIX syscall interface + /proc, /sys, /dev       │
└────────────────────────────┬─────────────────────────────────────┘
                             │ Hardware instructions, DMA, IRQs
┌────────────────────────────▼─────────────────────────────────────┐
│  Layer 0 — HARDWARE & FIRMWARE                                    │
│  What: CPU, GPU, RAM, storage, NIC, input devices, UEFI firmware │
│  Interface DOWN: physics                                           │
│  Interface UP: hardware instruction set, PCI/PCIe, UEFI services │
└──────────────────────────────────────────────────────────────────┘
```

---

## Layer Specifications

### Layer 0 — Hardware & Firmware

**Owns:**
- Physical CPU, GPU, RAM, NVMe, network card, USB controllers, input devices
- UEFI firmware (boot services, runtime services)
- GPU firmware (microcode, power management firmware)

**Does not own:**
- Any software above the kernel
- Display mode configuration (that is KMS)
- Filesystem layout (that is the kernel + userspace)

**Interface to Layer 1:** CPU instruction set (x86_64), hardware memory map, UEFI handoff, PCI device enumeration, interrupt lines, DMA channels.

**Mahina position:** Mahina does not modify firmware. Mahina does not flash GPU firmware. Mahina reads UEFI runtime variables (e.g., for Secure Boot status) but does not write them except through a dedicated firmware interface tool (Phase 5, Luna Performance Lab experimental mode).

---

### Layer 1 — Linux Kernel

**Owns:**
- Process scheduling (CFS, cgroup v2 hierarchies)
- Virtual memory (paging, mmap, CoW, swap)
- Filesystem VFS layer (ext4/btrfs on top of it)
- Device drivers (GPU, NIC, input, storage, audio)
- Network stack (TCP/IP, netfilter)
- Security enforcement (AppArmor, capabilities, seccomp)
- IPC primitives (pipes, sockets, shared memory, signals)
- DRM/KMS subsystem (display hardware management)
- /dev node management (devtmpfs)

**Does not own:**
- Display compositing (that is Layer 3)
- User sessions (that is Layer 2 — luna-init)
- Application data (that is Layer 4/5)
- AI inference (that is Layer 2 — luna-ai-d)

**Interface to Layer 2:** POSIX syscall ABI (open/read/write/mmap/socket/ioctl...), /proc virtual filesystem, /sys virtual filesystem, /dev device nodes, kernel signals.

**Boundary rule:** Userspace processes communicate with the kernel exclusively through syscalls and file reads/writes to /proc, /sys, /dev. No userspace process reads raw kernel memory. No userspace process calls kernel functions directly.

---

### Layer 2 — System Services

**Owns:**
- System startup and service supervision (`luna-init`)
- IPC bus (`D-Bus`)
- Network connection management (`NetworkManager`)
- Audio routing (`PipeWire`)
- Time synchronization (`ntpd`)
- AI runtime (`luna-ai-d` — both Presence Engine and LLM Inference Engine)
- Firewall rules (`nftables`, configured by luna-init)
- Package management daemon (lpkg backend)
- Cgroup slice management (luna-init creates all cgroup slices)

**Does not own:**
- Display output — System services never open /dev/dri directly
- Window management — system services do not create graphical surfaces
- Input events — system services do not read from /dev/input directly
- Any POSIX file below /proc, /sys, /dev except:
  - /dev/dri — only the compositor (Layer 3) opens this
  - /dev/input — only libinput via the compositor opens these
  - /dev/dma_heap — only the compositor + direct-LGP clients open this

**Interface to Layer 3 (graphics):**
- D-Bus signals (compositor readiness, theme changes, session events)
- /run/luna-compositor-ready sentinel file (or D-Bus equivalent)
- luna-ai-d connects to the LGP compositor as an LGP client (like any other client) — it does not have special kernel-level access to the graphics layer

**Interface to Layer 4 (userland):**
- D-Bus service APIs (luna-ai-d conversation API, luna-notif notification API, lpkg install API)
- /run/ socket files (luna-init management socket)
- POSIX files in /var/lib/, /etc/luna/, ~/.luna/

**Boundary rule:** System services do not create graphical surfaces directly — they communicate through the LGP protocol like any other client. `luna-ai-d` is a Layer 2 service that happens to be an LGP client. It does not bypass the compositor. It does not talk to the GPU.

---

### Layer 3 — Graphics

**Owns:**
- LGP compositor process (`lgp-compositor`)
- GPU abstraction layer (`lgp-render`)
- DRM/KMS interface (exclusive — no other process opens /dev/dri)
- libinput device management (exclusive — compositor reads all input, routes to clients)
- Animation engine (inside compositor)
- Theme engine (inside compositor — Color Resolver, typography values)
- Frame timing (vsync, frame callbacks)
- Surface lifetime management (create, destroy, Z-order)
- Input routing (keyboard focus, pointer hit-testing)

**Does not own:**
- Application logic (that is Layer 4/5)
- Session management (that is Layer 2 — luna-init)
- AI model inference (that is Layer 2 — luna-ai-d)
- File I/O beyond its own config and logs
- Network access

**Interface to Layer 4 (userland):**
- LGP Unix socket at `/run/lgp/compositor.sock`
- LGP protocol messages (surface creation, buffer commit, input delivery, frame callbacks)
- `LGP_THEME_CHANGED` broadcast event

**Boundary rule:** The compositor does not call into Layer 4 code. It is not a library linked into applications. It is a process that communicates with applications through the LGP socket. If you are writing compositor code that imports a LunaGUI header, you have crossed the boundary.

---

### Layer 4 — Userland

**Owns:**
- LunaGUI toolkit (library linked into applications)
- luna-shell (desktop root surface)
- luna-bar (status bar surface)
- luna-island (LUNA visual surface — an LGP client, not part of the compositor)
- luna-notif (notification daemon — an LGP client)
- lpkg (package manager user-facing client)
- Settings application
- All built-in Mahina applications
- File manager, text editor, terminal emulator, etc.

**Does not own:**
- DRM/KMS — Layer 4 components never open /dev/dri
- Input devices — Layer 4 receives input via LGP_INPUT_EVENT, not from /dev/input
- D-Bus service hosting for core services (D-Bus is owned by Layer 2)
- luna-ai-d — luna-island is a consumer of luna-ai-d's API, not an owner of it

**Interface to Layer 5 (applications):**
- LunaGUI C API (widget creation, canvas drawing, animation)
- LGP direct socket (for applications that bypass LunaGUI)
- POSIX standard I/O (files, pipes, stdin/stdout for terminal applications)

**Boundary rule:** Layer 4 components call into Layer 3 (compositor) via LGP messages and into Layer 2 (system services) via D-Bus. They do not call kernel interfaces directly except for POSIX file I/O. They do not open DRM devices. They do not manage processes.

---

### Layer 5 — Applications

**Owns:**
- User-chosen software installed via lpkg or directly
- All application logic, data, and UI
- Their own LGP surfaces (via LunaGUI or direct-LGP)
- Their own filesystem data in ~/.local/ (per-user install, DL-017)

**Does not own:**
- System services
- Compositor surfaces belonging to other applications
- Other applications' memory or filesystem data
- luna-ai-d inference directly — applications request via LUNA API (Layer 4 mediated)

**Boundary rule:** Applications are sandboxed per DL-020 (Phase 5). They cannot escalate privilege without going through the LUNA permission dialog (Layer 4 → Layer 2 via D-Bus).

---

## State Machine: System Call Flow

```
Application (Layer 5)
  │
  │  widget.animate(LunaAnimation.FadeIn)
  ▼
LunaGUI (Layer 4)
  │
  │  LGP_SEND_MOTION {surface_id: N, motion_class: Fade}
  │  [write to /run/lgp/compositor.sock]
  ▼
lgp-compositor (Layer 3)
  │
  │  Validates motion class
  │  Adds to animation engine
  │  Next frame: applies opacity transform
  │  Renders to GPU framebuffer
  │
  │  [write() to DRM fd via KMS ioctl]
  ▼
Linux kernel DRM/KMS (Layer 1)
  │
  │  [schedules page flip at next vblank]
  ▼
Hardware GPU / display controller (Layer 0)
  │
  │  [scans out framebuffer to display]
  ▼
Pixels on screen
```

```
Application requests package install
  │
  │  lpkg_client.install("package-name")
  ▼
lpkg client (Layer 4)
  │
  │  D-Bus message: org.mahina.lpkg.Install("package-name")
  ▼
lpkg daemon (Layer 2)
  │
  │  Checks permissions (DL-016)
  │  If privilege required: sends LGP message to compositor
  │                          for LUNA permission dialog
  │  Permission granted: executes download + verify + install
  │
  │  [open/write syscalls for package files]
  ▼
Linux kernel VFS (Layer 1)
  │
  │  [writes files to ext4/btrfs]
  ▼
NVMe storage (Layer 0)
```

---

## Responsibility Matrix

| Responsibility | Layer 0 | Layer 1 | Layer 2 | Layer 3 | Layer 4 | Layer 5 |
|---|---|---|---|---|---|---|
| Process scheduling | | ✅ | | | | |
| Service supervision | | | ✅ luna-init | | | |
| Display output | | | | ✅ compositor | | |
| Input events | ✅ hardware | ✅ kernel driver | | ✅ compositor routes | ✅ receives | ✅ receives |
| GPU command submission | | | | ✅ lgp-render | | |
| Window creation | | | | ✅ surface manager | ✅ requests via LGP | ✅ requests via LGP |
| Color resolution | | | | ✅ Color Resolver | | |
| Animation timing | | | | ✅ animation engine | ✅ requests via API | |
| Theme values | | | | ✅ theme engine | ✅ reads via LunaTheme | |
| AI inference | | | ✅ luna-ai-d | | | |
| Package management | | | ✅ lpkg daemon | | ✅ lpkg client | ✅ calls lpkg |
| Networking | | ✅ network stack | ✅ NetworkManager | | | |
| Audio | | ✅ ALSA driver | ✅ PipeWire | | | |
| IPC bus | | | ✅ D-Bus | | | |
| Security enforcement | | ✅ AppArmor | | | | |
| Filesystem | | ✅ VFS | | | | |
| Memory management | | ✅ kernel mm | | | | |
| User sessions | | | ✅ luna-init | | | |

---

## Boundary Violations — Explicit Prohibition List

The following are **never permitted** in Mahina code:

| Action | Violates | Why |
|---|---|---|
| Application opens `/dev/dri/card0` directly | Layer 5 → Layer 1 skip | Only compositor owns DRM |
| Application reads from `/dev/input/event*` directly | Layer 5 → Layer 1 skip | Input is compositor-routed |
| Compositor hosts a D-Bus service | Layer 3 → Layer 2 boundary | Compositor is stateless to services |
| luna-ai-d opens `/dev/dri` | Layer 2 → Layer 1 skip | AI has no business with GPU hardware |
| LunaGUI toolkit spawns threads that call kernel ioctls | Layer 4 syscall bypass | All kernel access via explicit POSIX calls only |
| Application writes to `/etc/` directly | Layer 5 → Layer 1 skip | Package manager owns /etc/ writes |
| compositor calls lua-ai-d directly (not via LGP) | Layer 3 → Layer 2 skip | All communication via LGP + D-Bus |
| System service creates an LGP surface without connecting as a client | Layer 2 → Layer 3 bypass | All surface creation goes through the LGP socket |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **luna-island layer.** Luna Island is a Layer 4 component (an LGP client). But it is more privileged than normal applications — it has access to `LUNA_ISLAND` surface type. Is this a Layer 4 privilege escalation or does luna-island belong in a "Layer 4.5" privileged userland tier? Must be resolved before luna-island implementation.

2. **lpkg daemon privilege.** The lpkg daemon runs as root (or a privileged user) to install system-wide packages. Is it Layer 2 or does it straddle Layer 2/4? Currently classified as Layer 2 (system service). Confirm.

3. **PipeWire** is Layer 2 in this document. Audio applications talk to PipeWire via its socket API. Does this match the Layer 2 / Layer 4 boundary? (Yes — PipeWire is a system service, applications are its clients. Consistent with how NetworkManager and D-Bus work.)

4. **luna-notif.** Notification daemon — is it Layer 2 or Layer 4? It is an LGP client (Layer 4 behavior) but provides a D-Bus service (Layer 2 behavior). Currently classified as Layer 4 but this dual role should be explicitly decided.

---

## AI Context

- When implementing any component, check this document first. Identify which layer the component belongs to. Then check: what interfaces does that layer expose, and what interfaces does it consume?
- A component that is opening a device node it isn't supposed to own is a boundary violation. Stop and re-design.
- The compositor (Layer 3) does not call into Layer 2 system services directly during the render loop. It may log to a file (Layer 1 syscall), but it does not call D-Bus in the hot path.
- `luna-ai-d` is Layer 2. It communicates with Layer 3 (compositor) through the LGP socket — as a normal LGP client. It does not have special access to Layer 3 internals.
- Applications (Layer 5) receive input events from the compositor (Layer 3) via LGP. They do not read `/dev/input`. If you see an application opening a raw input device, it is a boundary violation.
- The LunaGUI toolkit (Layer 4) is a library, not a process. It links into the application. The compositor (Layer 3) is a process. These are different architectural entities — a library and a server.

---

*Document: `Volume II / 12_kernel_user_boundary.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Classification: Canonical — boundary definitions here take precedence over any volume-local assumptions*
*Cross-references: all Volume II–V documents. Every component's layer assignment is defined here.*
