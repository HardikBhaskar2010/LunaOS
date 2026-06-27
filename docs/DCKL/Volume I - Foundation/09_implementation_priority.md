# Mahina — Implementation Priority
**Volume I · Chapter 9**
**Classification:** Foundation Document — Build Order Authority
**Status:** Canonical · No component may be implemented before its prerequisites in this document

---

## Purpose

This document defines the **mandatory build order** for Mahina. It answers the question: "What do we build first so the system can boot, and what comes after?"

Without this document, engineers will accidentally implement the Theme Engine before the Surface Manager, or the Animation Engine before DRM/KMS. This document prevents that.

This is not a roadmap. It is a dependency graph translated into phases.

---

## Overview

Mahina is built in five phases. Each phase produces a runnable milestone — something you can actually boot and test. No phase may begin until the previous phase's **required deliverables** are complete.

```
Phase 0 — Bare Metal
    │  A machine that boots and gives you a shell.
    ▼
Phase 1 — System Foundation
    │  A machine with a supervised init, networking, and packages.
    ▼
Phase 2 — Graphics Primitive
    │  A machine that displays a rectangle on screen.
    ▼
Phase 3 — Living Desktop
    │  A machine with a working compositor, shell, and LUNA Presence.
    ▼
Phase 4 — Full Presence
    │  A machine where LUNA can converse, reason, and remember.
    ▼
Phase 5 — Polish & Completeness
       Themes, accessibility, performance lab, extended applications.
```

**This is the milestone order. DL-024 (Success Criteria) is achieved between Phase 3 and Phase 4.**

---

## Architecture

### Dependency Graph

```
┌─────────────────────────────────────────────────────────────────┐
│  PHASE 0 — BARE METAL                                            │
│                                                                   │
│  ① Linux kernel (Mahina config)                                   │
│  ② libc (glibc v1)                                               │
│  ③ limine bootloader                                              │
│  ④ initramfs + root filesystem (ext4 or btrfs)                   │
│  ⑤ luna-init (C, PID 1) — Stages 0–3 only                       │
│     └── No services. Just: boot, mount, shell.                   │
└───────────────────────────┬─────────────────────────────────────┘
                            │ all above required
┌───────────────────────────▼─────────────────────────────────────┐
│  PHASE 1 — SYSTEM FOUNDATION                                      │
│                                                                   │
│  ① luna-init Stage 4 services:                                    │
│     ├── nftables (firewall)                                       │
│     ├── D-Bus daemon                                              │
│     ├── NetworkManager                                            │
│     ├── PipeWire                                                  │
│     └── ntpd                                                      │
│  ② lpkg (package manager) — install/remove only                  │
│  ③ lpkg repository: first-party packages                          │
│  ④ Kernel module loading (GPU drivers, input drivers)             │
└───────────────────────────┬─────────────────────────────────────┘
                            │ all above required
┌───────────────────────────▼─────────────────────────────────────┐
│  PHASE 2 — GRAPHICS PRIMITIVE                                     │
│                                                                   │
│  ① DRM/KMS device open + mode set                                │
│  ② lgp-render GPU abstraction layer (Vulkan or EGL stub)         │
│  ③ Framebuffer allocation + KMS page flip (ONE rectangle on screen│
│  ④ LGP compositor: surface manager ONLY                           │
│     ├── LGP socket (/run/lgp/compositor.sock)                    │
│     ├── CREATE_SURFACE / DESTROY_SURFACE messages                 │
│     ├── COMMIT_BUFFER (shm only, no DMA-BUF yet)                 │
│     └── KMS scanout of the composited framebuffer                 │
│  ⑤ Boot splash (framebuffer, luna-init renderer)                 │
│  ⑥ Input: libinput reading keyboard + pointer events             │
│                                                                   │
│  ✓ MILESTONE: "We can draw a window on screen."                   │
└───────────────────────────┬─────────────────────────────────────┘
                            │ all above required
┌───────────────────────────▼─────────────────────────────────────┐
│  PHASE 3 — LIVING DESKTOP                                         │
│                                                                   │
│  ① LGP compositor: add remaining protocol                         │
│     ├── Semantic color resolution (Color Resolver)                │
│     ├── Motion class validation + enforcement                     │
│     ├── Z-order layer system                                      │
│     ├── Input routing (keyboard focus, pointer hit-test)          │
│     └── Frame callbacks (vsync-aligned LGP_FRAME_CALLBACK)       │
│  ② Animation engine (inside compositor)                           │
│     ├── All 9 motion classes                                      │
│     ├── Animation Budget enforcement                              │
│     └── Complexity budget                                         │
│  ③ LunaGUI toolkit (C library)                                    │
│     ├── Widget tree + layout engine (box model)                   │
│     ├── Core widget set (see 04_lunagui.md)                       │
│     ├── Canvas API                                                │
│     └── Animation API → LGP motion mapping                       │
│  ④ luna-shell (root desktop surface)                              │
│  ⑤ luna-bar (status bar)                                          │
│  ⑥ luna-ai-d: Presence Engine ONLY (no LLM)                      │
│     ├── Context engine (lightweight, reads observe.toml)          │
│     └── Mode detection (AMBIENT, DEVSHELL, FOCUS, etc.)          │
│  ⑦ luna-island (Luna Island surface — static, no Live2D)         │
│  ⑧ luna-notif (notification daemon)                               │
│  ⑨ Theme Engine: Luna Dark built-in defaults ONLY                │
│     └── (no user theme loading yet — hardcoded defaults)         │
│                                                                   │
│  ✓ MILESTONE: "We have a working desktop. LUNA is present."       │
│  ✓ DL-024 Phase 3 checkpoint: Does it feel alive?                │
└───────────────────────────┬─────────────────────────────────────┘
                            │ all above required
┌───────────────────────────▼─────────────────────────────────────┐
│  PHASE 4 — FULL PRESENCE                                          │
│                                                                   │
│  ① luna-ai-d: LLM Inference Engine                                │
│     ├── Ollama integration (lazy start on first demand)           │
│     ├── Conversation API                                          │
│     └── luna bridge (cloud fallback)                              │
│  ② Memory Engine: persistent memory + shutdown summarization      │
│     └── Encrypted persistent_summary.enc (DL-023)               │
│  ③ Permission Engine: install-time observe.toml setup            │
│  ④ Three-Strike Rule enforcement                                  │
│  ⑤ Theme Engine: full TOML theme loading + switching             │
│  ⑥ Theme picker UI (LunaGUI settings application)               │
│  ⑦ DMA-BUF support (CANVAS_SURFACE with GPU rendering)           │
│  ⑧ lpkg: full feature set                                         │
│     ├── Per-user install (DL-017)                                 │
│     ├── Atomic transactions (DL-018)                              │
│     ├── Signature verification (DL-019)                           │
│     └── LUNA graphical permission interface (DL-016)              │
│  ⑨ Snapshot support: pre-update snapshots (DL-011)               │
│                                                                   │
│  ✓ MILESTONE: "LUNA can think, remember, and converse."           │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────────┐
│  PHASE 5 — POLISH & COMPLETENESS                                  │
│                                                                   │
│  ① Accessibility: keyboard navigation, reduced-motion            │
│  ② Luna Performance Lab (Discussion_Session_3.md → Volume V)     │
│  ③ Third-party app sandboxing (DL-020)                            │
│  ④ LunaGUI: Rust binding                                          │
│  ⑤ Community repositories + reputation system (DL-019)           │
│  ⑥ Screen reader support                                          │
│  ⑦ Secure Boot (limine signing)                                   │
│  ⑧ Hibernate / resume                                             │
│  ⑨ Multi-GPU support                                              │
│  ⑩ Hardware overlay planes                                        │
│  ⑪ Adaptive sync (VRR)                                            │
│  ⑫ HDR output                                                     │
│                                                                   │
│  ✓ MILESTONE: "v1.0 release candidate."                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Current Decisions

### Phase 2 is the highest-risk phase

Phase 2 is the first time Mahina writes a pixel to screen using its own graphics stack. It has no upstream precedent to lean on — LGP is custom, DRM/KMS integration is custom, the GPU abstraction layer is custom. This phase will take the most engineering time relative to its visible output.

**The minimum Phase 2 deliverable is:** a compositor that can open a DRM device, configure a display mode via KMS, accept a single shm buffer from a client, and composite it to screen.

Everything else — animation, semantic colors, input routing — comes later in Phase 3.

### Do not build the Theme Engine in Phase 2 or Phase 3 entry

The Theme Engine (full TOML loading + switching) is a Phase 4 deliverable. **Luna Dark defaults should be hardcoded into the compositor during Phase 2 and Phase 3.** Replace hardcoded defaults with TOML loading in Phase 4.

Rationale: building the theme system before the compositor is stable wastes effort. Theme loading has no effect if surfaces aren't rendering.

### Animation Engine comes after the Surface Manager

The animation engine (inside the compositor) requires:
- Surface manager (to know which surfaces exist) — Phase 2
- Frame callbacks (to advance per-frame) — Phase 3 entry
- Motion class validation (LGP protocol) — Phase 3 entry

Animation engine implementation starts in Phase 3. The compositor should have frame callbacks working before any animation code is written.

### LunaGUI comes after the compositor accepts connections

LunaGUI is a client of LGP. You cannot develop LunaGUI without a compositor to connect to. The Phase 2 compositor (socket + surface creation + shm commit) is the minimum LunaGUI development target.

### LUNA Presence Engine comes before LLM

The Presence Engine (context, mode, luna-island) is a Phase 3 deliverable. Ollama and LLM inference are Phase 4. Never reverse this order. A desktop that feels alive without conversational AI is better than a desktop that can converse but feels empty.

---

## Implementation Priority Table

For each component, the phase in which it must be complete before the next phase begins:

| Component | Phase | Blocks |
|---|---|---|
| linux kernel (Mahina config) | 0 | everything |
| limine | 0 | everything |
| luna-init (Stages 0–3) | 0 | everything |
| D-Bus | 1 | luna-ai-d IPC, compositor readiness signal |
| NetworkManager | 1 | package downloads, time sync |
| lpkg (install/remove) | 1 | all further software installation |
| DRM device open + KMS mode set | 2 | compositor, display output |
| lgp-render GPU layer | 2 | compositor rendering |
| LGP compositor: surface manager | 2 | ALL graphical clients |
| LGP compositor: shm buffer commit | 2 | all LunaGUI + direct-LGP apps |
| Boot splash | 2 | visual continuity during boot |
| libinput integration | 2 | all user input |
| LGP compositor: frame callbacks | 3 | animation engine, LunaGUI vsync |
| LGP compositor: Color Resolver | 3 | semantic color display |
| LGP compositor: motion validation | 3 | animation engine |
| LGP compositor: Z-order layers | 3 | luna-island above apps |
| LGP compositor: input routing | 3 | user interaction with apps |
| Animation engine | 3 | all motion |
| LunaGUI widget core | 3 | all LunaGUI applications |
| luna-shell | 3 | desktop surface |
| luna-bar | 3 | status display |
| luna-ai-d (Presence Engine) | 3 | LUNA mode, context, expressions |
| luna-island (static) | 3 | LUNA visual presence |
| luna-notif | 3 | notification display |
| Luna Dark hardcoded defaults | 3 | visual appearance during dev |
| Ollama + LLM Inference Engine | 4 | LUNA conversation |
| Memory Engine (persistent) | 4 | LUNA memory across reboots |
| Theme Engine (TOML) | 4 | user-selectable themes |
| lpkg: atomic transactions | 4 | safe package management |
| lpkg: signature verification | 4 | secure packages |
| DMA-BUF (CANVAS_SURFACE GPU) | 4 | games, video |
| Luna Performance Lab | 5 | power user feature |
| Third-party sandboxing | 5 | security hardening |
| Accessibility | 5 | inclusivity |

---

## Failure Mode: What happens if this order is violated?

| Violation | Consequence |
|---|---|
| Build Animation Engine before Surface Manager | Animation references surface IDs that don't exist |
| Build Theme TOML loader before compositor renders | Theme changes apply to nothing — wasted effort |
| Build LLM before Presence Engine | LUNA can converse but the desktop feels empty — inverts DL-024 |
| Build LunaGUI before LGP compositor accepts connections | No test target — development is blind |
| Build luna-shell before luna-island | Shell starts but LUNA doesn't exist on it — partial experience |
| Build lpkg signature verification before lpkg installs | Security feature that guards no functionality |

---

## Open Questions

1. **Phase 2 GPU backend choice.** Should Phase 2 start with a software renderer (fastest to implement, no GPU required) and upgrade to Vulkan/EGL in Phase 3? This is an attractive approach — software render proves the pipeline before GPU complexity is added. Must be a Decision Log entry before Phase 2 begins.

2. **Phase 3 split.** Phase 3 is large. It may need to be split into Phase 3a (compositor complete, no LunaGUI) and Phase 3b (LunaGUI + shell + LUNA). If the Phase 3 scope is too large for one milestone, this split should be formalized.

3. **Phase ordering for lpkg vs. compositor.** lpkg is a Phase 1 deliverable but many lpkg features (graphical permission interface) require Phase 3 compositor. The Phase 1 lpkg is terminal-only. The graphical lpkg features are Phase 4. This is correct — verify it matches DL-016 (graphical privilege escalation).

---

## AI Context

- Build order is: Phase 0 → 1 → 2 → 3 → 4 → 5. No skipping. No parallelism across phases for components that have phase dependencies.
- Phase 2 ends when a pixel is on screen from a compositor-managed surface. Not before.
- Phase 3 ends when a LUNA presence indicator is visible on the desktop and the user can interact with a shell. Not before.
- The Theme Engine (TOML loading) is Phase 4. If you are in Phase 2 or 3 and you feel the urge to implement theme file loading, stop. Hardcode Luna Dark values and continue.
- The LLM Inference Engine (Ollama) is Phase 4. The Presence Engine is Phase 3. These are different components with different priorities.
- This document supersedes any other ordering implied by Volume III or IV documents. If a Volume III document says "LunaGUI must be built first," it is wrong — the compositor must be built first (Phase 2), then LunaGUI (Phase 3).

---

*Document: `Volume I / 09_implementation_priority.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Classification: Canonical — this document is the build order authority*
*Cross-references: All Volume II–V documents. Phase order governs all implementation timelines.*
