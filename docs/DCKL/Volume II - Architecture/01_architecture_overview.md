# LunaOS — Architecture Overview
**Volume II · Chapter 1**
**Classification:** Core Architecture — System Design
**Status:** Active · Canonical Reference

---

## Purpose

This document provides the structural overview of the LunaOS software stack. It defines how every major subsystem relates to every other subsystem, establishes the communication topology, and serves as the entry point for all Volume II technical documentation.

Every subsequent Volume II document describes one subsystem in detail. This document describes the whole.

---

## Overview

LunaOS is a Linux-based operating system built from intentional component choices with no inherited base distribution. Every layer — from bootloader to desktop shell — was selected or written explicitly for this project.

### Non-Negotiable Constraints

The following architectural constraints are permanently fixed and may never be overridden by any Volume II or later document:

| Constraint | Status | Source |
|---|---|---|
| No Wayland | ❌ Prohibited | `non_negotiables.md` |
| No Hyprland | ❌ Prohibited | `non_negotiables.md` |
| No GNOME | ❌ Prohibited | `non_negotiables.md` |
| No KDE | ❌ Prohibited | `non_negotiables.md` |
| LGP (Luna Graphics Protocol) | ✅ Required | `non_negotiables.md` |
| Local-first AI | ✅ Required | `non_negotiables.md`, Core Law II |
| Rolling Release | ✅ Required | `non_negotiables.md` |
| LUNA is a Digital Presence | ✅ Required | `non_negotiables.md`, `identity.md` |
| Motion communicates state | ✅ Required | `non_negotiables.md`, Core Law III |

Any document, AI agent, or contributor that introduces Wayland, Hyprland, GNOME, or KDE into LunaOS documentation or code violates project law. These are not subject to discussion or future decision log entries.

---

## Design Philosophy

The architecture derives directly from the Core Laws and the documentation hierarchy:

```
Vision → Philosophy → Core Laws → Identity → Architecture → Subsystems → Implementation → Code
```

Every architectural decision is recorded in `decision_log.md`. No subsystem may be added to LunaOS without a corresponding Decision Log entry answering: what does it do, why this choice, what does it touch, what maintains it.

**Key design constraints from Core Laws:**

1. **Own Every Layer (Law I).** No mystery processes. Every process running on LunaOS was explicitly started by `luna-init` or is a direct child of an explicitly started process.

2. **Local First (Law II).** Every core feature works without internet connectivity. AI inference is local. Cloud is opt-in only.

3. **Aesthetic Is Functional (Law III).** Every animation communicates real system state. Every color follows the locked Color Semantic Contract. Every motion follows the locked Motion Vocabulary.

4. **Silence Before Suggestion (Law IV).** LUNA.AI does not interrupt unless confidence exceeds threshold. Observation is deny-by-default.

5. **User Owns the Machine (Law V).** No irreversible action without explicit confirmation. No telemetry. No automatic updates.

6. **Documentation Is Code (Law VI).** Every component has a corresponding document before implementation begins.

---

## Architecture

### Layer Stack

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 6 — LUNA AI Layer                                        │
│  luna-ai-d · presence engine · personality engine              │
│  context engine · memory engine · permission engine            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5 — Userland / Shell                                     │
│  luna-shell · luna-bar · luna-notif · luna-island              │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4 — Graphics / LGP / LunaGUI                             │
│  Luna Graphics Protocol (LGP) · LunaGUI toolkit                │
│  custom compositor · animation engine · theme engine           │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3 — System Services                                      │
│  luna-init · PipeWire · NetworkManager · D-Bus                 │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2 — Package / Build Infrastructure                       │
│  lpkg · LBUILD · package repository                            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 1 — Kernel / Hardware                                    │
│  Linux 6.6.x LTS · glibc (v1) → musl (v2) · limine bootloader │
└─────────────────────────────────────────────────────────────────┘
```

Lower layers have no knowledge of higher layers. Higher layers depend on lower layers only through defined interfaces.

### Layer 1 — Kernel and Bootloader

| Component | Choice | Notes |
|---|---|---|
| Bootloader | limine | UEFI-first, minimal config format (DL-005) |
| Kernel | Linux 6.6.x LTS | Pinned version, security patches only (DL-009) |
| C Library | glibc (v1) → musl (v2) | musl migration is a v2.0 milestone (DL-007) |
| Architecture | x86_64 primary | ARM64 future target — not in scope for v1 |

The kernel configuration is tracked in `kernel/.config` with comments in `kernel/.config.notes`. Every enabled option has a written justification. See `03_linux_architecture.md` for full kernel configuration documentation.

### Layer 2 — Package and Build Infrastructure

| Component | Description |
|---|---|
| `lpkg` | Custom package manager, Python 3.12 (v1), Rust (v2 planned), SQLite database |
| LBUILD files | Package build recipes, TOML format |
| Package repository | Static file server, self-hosted |

`lpkg` handles installation, removal, dependency resolution, and system upgrade. See `Volume V / 03_package_manager.md` for full specification.

### Layer 3 — System Services

`luna-init` is PID 1. Written in C. Manages the complete service lifecycle.

| Service | Role |
|---|---|
| `luna-init` | PID 1, service supervisor, TOML service files |
| `NetworkManager` | Network management |
| `PipeWire` | Audio / video routing |
| `D-Bus daemon` | IPC bus for system services |
| `luna-ai-d` | LUNA.AI daemon, localhost only |

`luna-init` service files use TOML format. Each service declares: name, binary path, dependencies, restart policy, capability requirements. See `04_init_system.md` for service file specification.

### Layer 4 — Graphics, LGP, and LunaGUI

The LunaOS graphics stack is built around two related but distinct components (DL-004R — hybrid graphics architecture):

**Luna Graphics Protocol (LGP):** The foundational graphics protocol between the compositor and all graphical clients. Exposes the Color Semantic Contract, Motion Vocabulary, and Animation Budget as protocol-level primitives. Advanced applications may use LGP directly.

**LunaGUI toolkit:** The standard high-level application interface. LunaGUI uses LGP internally. Normal applications communicate through LunaGUI — they do not need to know LGP details. This preserves simplicity for application developers while allowing high-performance software to bypass to LGP directly.

```
TODO:
Decision not yet finalized.
Reason: LGP protocol wire format has not been specified.
Volume III / 01_lgp.md must define:
  - LGP protocol format (compositor-facing)
  - LunaGUI toolkit API surface (application-facing)
  - The boundary between LunaGUI and direct-LGP access
This volume documents the architectural roles without specifying formats.
```

What is decided (DL-004R):
- LGP is the graphics protocol. No Wayland protocol is used.
- LunaGUI is the standard application toolkit, built on LGP.
- A custom LunaOS-written compositor is the rendering target.
- Luna Island is a native compositor surface.
- Advanced applications may use LGP directly without LunaGUI.

What is not yet decided:
- LGP wire format (Volume III / 01_lgp.md).
- LunaGUI API surface (Volume III — separate document).
- GPU acceleration backend (Vulkan, OpenGL, or software renderer).

### Layer 5 — Userland Shell

The LunaOS desktop shell is a set of independent processes coordinated through defined IPC channels.

| Component | Role |
|---|---|
| `luna-shell` | Root desktop surface, wallpaper, layout |
| `luna-bar` | Status bar: time, network, audio, tray |
| `luna-island` | LUNA presence widget, notifications, primary user-facing AI surface |
| `luna-notif` | Notification daemon, notification history |
| `luna-lock` | Lock screen |

Each component is a standalone process that communicates with the LGP compositor through the defined LGP protocol. Inter-component communication uses D-Bus.

```
TODO:
Decision not yet finalized.
Reason: Shell-to-compositor IPC mechanism depends on LGP specification.
Until Volume III / 01_lgp.md defines LGP's protocol, shell component
communication details cannot be finalized.
```

### Layer 6 — LUNA AI Layer

Per DL-021, LUNA consists of two independent systems with different startup behaviors:

**LUNA Presence Engine** — starts at boot (Stage 6), always running:

| Component | Function |
|---|---|
| Presence Engine | Determines LUNA's active mode (DEVSHELL, FOCUS, MEDIA, AMBIENT, CONVERSATION, CRISIS) |
| Personality Engine | Selects tone and response style based on active mode |
| Context Engine | Aggregates current system state — lightweight, no LLM |
| Memory Engine | Reads/writes `~/.luna/memory/` — pattern history, user preferences |
| Permission Engine | Enforces observation allow-list from `~/.luna/config/observe.toml` |

The Presence Engine handles Luna Island, context awareness, expressions, notifications, and lightweight behavior. It is always running after boot. Target footprint: under 100 MB RAM.

**LLM Inference Engine** — loads lazily, on first demand:

| Trigger | Action |
|---|---|
| User starts a conversation | Inference engine initializes, Ollama starts, model loads |
| Voice interaction begins | Same as above |
| AI automation requested | Same as above |
| Explicit reasoning required | Same as above |

The LLM Inference Engine and Ollama are **not started at boot**. They initialize on first demand. This eliminates the 2–3 GB Ollama RAM footprint during normal desktop operation when the user is not actively conversing with LUNA.

Both systems together form `luna-ai-d`. Their separation is a runtime behavior distinction, not necessarily a process separation (implementation detail deferred to Volume IV).

---

## Communication Topology

```
TODO:
Decision not yet finalized.
Reason: Full IPC topology depends on LGP protocol specification (Volume III).
The topology below represents what is decided. Undecided paths are marked TODO.
```

What is decided:

```
User Applications
        │
        │ [LGP protocol — format TODO]
        ▼
LGP Compositor (custom)
        │
        │ D-Bus
        ▼
System Services (NetworkManager, PipeWire, etc.)
        │
        │ luna-init supervises all
        ▼
luna-init (PID 1)
        │
        │ Linux syscalls
        ▼
Linux 6.6.x Kernel

luna-ai-d ◄── [IPC to shell — format TODO] ──► luna-island
                                                  luna-bar
                                                  luna-notif

luna-ai-d ◄── Ollama REST (localhost:11434) ──► Ollama daemon
```

D-Bus is used for system-level events. The protocol between `luna-ai-d` and the shell components is not yet finalized and depends on Volume IV architecture decisions. PipeWire handles all audio/video media routing through its own Unix socket protocol.

---

## Current Decisions

All decisions are recorded in `decision_log.md`. Decisions affecting architecture at this level:

| DL ID | Decision |
|---|---|
| DL-001 | No upstream distro base — LFS approach |
| DL-002 | luna-init in C, TOML service files |
| DL-003 | lpkg in Python (v1), Rust planned (v2) |
| DL-004R | Hybrid graphics: LunaGUI toolkit + direct LGP access (supersedes DL-004) |
| DL-005 | limine bootloader |
| DL-006 | Ollama for local AI inference |
| DL-007 | glibc (v1) → musl (v2) |
| DL-008 | TOML as universal config format |
| DL-009 | Linux 6.6.x LTS kernel |
| DL-010 | luna-ai-d on localhost:7734 |
| DL-021 | AI Runtime: Presence Engine (boot) and LLM Inference Engine (lazy) |

Note: DL-004 is superseded by DL-004R (Architecture Review Meeting #2). The graphics architecture is hybrid: LunaGUI for standard applications, direct LGP for advanced applications. A custom LunaOS compositor is the rendering target. This is now formally recorded in `decision_log.md`.

Note: DL-021 (AI Runtime) splits LUNA into Presence Engine (boot-time) and LLM Inference Engine (lazy). Ollama is not a boot-time service.

---

## Technical Details

### Minimum Hardware Requirements (v1 target)

| Component | Minimum | Recommended |
|---|---|---|
| CPU | x86_64, 2 cores | 4+ cores |
| RAM | 4 GB | 8 GB (Ollama holds model in RAM) |
| Storage | 20 GB | 40 GB |
| GPU | Any with framebuffer support | Hardware-accelerated rendering (specifics depend on LGP GPU backend decision) |
| Firmware | UEFI | UEFI |

```
TODO:
Decision not yet finalized.
Reason: GPU minimum requirement depends on LGP rendering backend decision.
RAM minimum depends on which Ollama model is bundled at install time.
Both must be resolved before hardware requirements are finalized.
```

### Service Startup Order

```
Stage 0: Kernel → luna-init alive (PID 1)
Stage 1: Filesystem mounts (/, /tmp, /dev, /proc, /sys)
Stage 2: Early hooks (hostname, clock, entropy)
Stage 3: D-Bus daemon
Stage 4: System services (NetworkManager, PipeWire) — depend on D-Bus
Stage 5: LGP compositor — depend on GPU/framebuffer ready
Stage 6: Shell layer (luna-shell, luna-bar, luna-island)
Stage 7: LUNA AI layer (Ollama, luna-ai-d)
```

Full timing targets per stage are in `02_boot_flow.md`.

### All User Configuration

- All user-specific LUNA data lives in `~/.luna/` (TOML files).
- All system configuration lives in `/etc/luna/` (TOML files).
- `luna-init` reads service files from `/etc/luna/services/`.
- `luna-ai-d` is the only process authorized to read `~/.luna/memory/`.
- No process except `luna-ai-d` shall read `~/.luna/`.

### Rolling Release

LunaOS follows a rolling release model. There are no periodic major version freezes after v1.0. Packages update continuously. System updates are manual (`lpkg update` — never automatic). Breaking changes require a new DCKL major version entry.

---

## Future Improvements

| Improvement | Target Version | Notes |
|---|---|---|
| musl libc migration | v2.0 | Smaller footprint, cleaner static linking |
| lpkg Rust rewrite | v2.0 | Performance and correctness |
| LGP full specification | v1.0 | Volume III first priority |
| Custom compositor v1 | v1.0 | Required for Luna Island |
| ARM64 port | Post v1.0 | Hardware target not yet finalized |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **LGP protocol direction.** Does the Luna Graphics Protocol sit between application and compositor, or between shell components and the compositor, or both? The answer determines whether LGP is an application framework, an internal rendering API, or both. Must be resolved in Volume III / 01_lgp.md before any compositor work begins.

2. **Compositor GPU backend.** Vulkan, OpenGL, or software renderer? Depends on LGP direction and target hardware GPU support. No decision recorded.

3. **luna-ai-d IPC to shell.** The protocol from `luna-ai-d` to `luna-island`, `luna-bar`, and `luna-notif` is not yet specified. Candidates include HTTP (localhost), D-Bus, or a custom Unix socket protocol. Must be recorded as a Decision Log entry.

4. **Minimum hardware specification.** GPU requirements and RAM minimums depend on decisions not yet made. Must be resolved before the installer (Volume V) is written.

5. **ARM64 roadmap.** The kernel config is currently x86_64 only. ARM64 support affects compiler toolchain, cross-compilation infrastructure, and hardware testing requirements.

6. **DL-004 supersession.** A formal Decision Log entry documenting the supersession of DL-004 (Hyprland) by the LGP + custom compositor decision must be created by the project owner.

---

## AI Context

An AI agent implementing LunaOS must observe these constraints absolutely:

- **No Wayland.** No Hyprland. No GNOME. No KDE. These are non-negotiable. Any document, code, or configuration referencing these systems violates project law and must be rejected.
- **LGP is the graphics protocol.** Until Volume III / 01_lgp.md defines LGP, no graphics protocol implementation decisions may be made. Mark all graphics IPC as TODO.
- **Rolling release.** There are no stable/unstable branches. All updates are rolling.
- **LUNA is a digital presence, not an application.** Do not design LUNA features as application features. LUNA expresses OS state through the Expression Layer (see `luna_personality.md`).
- **All color choices are governed by the Color Semantic Contract (`core_laws.md` Law III).** Do not introduce colors outside the five locked colors.
- **All animation types are governed by the Motion Vocabulary (`core_laws.md` Law III).** Do not introduce new motion types.
- **All user data is in `~/.luna/`.** Only `luna-ai-d` reads this directory.
- **Cloud is opt-in.** `luna-ai-d` never makes external network calls automatically.

---

*Document: `Volume II / 01_architecture_overview.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-draft*
*Depends on: vision.md, philosophy.md, core_laws.md, identity.md, decision_log.md, glossary.md, non_negotiables.md*
*Supersedes: v0.1-draft (contained Wayland/Hyprland references — non-compliant)*
