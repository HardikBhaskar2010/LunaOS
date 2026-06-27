# Mahina — Component Ownership Matrix
**Volume II · Chapter 13**
**Classification:** Core Architecture — Ownership Authority
**Status:** Canonical · If ownership is unclear, this document decides it

---

## Purpose

This document answers one question for every system capability: **who owns it?**

"Ownership" means: one component is responsible for the authoritative state of this capability. It initializes it, maintains it, and is the only component that may write to it directly. All other components are consumers — they read, request, or subscribe.

Without this document, multiple components will try to own the same capability and produce conflicting state. This is the document that prevents that.

---

## Overview

```
Ownership Rule: Every system capability has exactly one owner.
                No two components may write to the same authoritative
                state without going through the owner.

Consumer Rule:  Any component that is not the owner must request
                changes through the owner's published interface
                (D-Bus method, LGP message, or POSIX file write).

Reader Rule:    Any component may read public state without
                going through the owner, as long as the state
                is exposed as a readable interface.
```

---

## Ownership Table

| Capability | Owner | Owner Process | Interface | Consumers |
|---|---|---|---|---|
| Clipboard | luna-shell | `luna-shell` | LGP extension (future) / X11 clipboard compat | All GUI apps |
| Notifications | luna-notif | `luna-notif` | D-Bus: `org.freedesktop.Notifications` | All apps, luna-bar, luna-island |
| Window lifecycle | LGP compositor | `lgp-compositor` | LGP protocol | All LGP clients |
| Theme / active theme | Compositor (Color Resolver) | `lgp-compositor` | `LGP_THEME_CHANGED` event | All LGP clients, LunaGUI |
| Theme selection (which theme is active) | luna-settings | `luna-settings` | Writes `~/.luna/config/theme.toml`, sends D-Bus signal | compositor |
| Input routing | LGP compositor | `lgp-compositor` | `LGP_INPUT_EVENT` delivery | All LGP clients |
| Focus (keyboard focus surface) | LGP compositor | `lgp-compositor` | Internal compositor state | read via LGP focus events |
| Session (user login / logout) | luna-init | `luna-init` | D-Bus: `org.lunaos.session.*` | All services |
| Display outputs (monitors) | LGP compositor | `lgp-compositor` | KMS (kernel) → LGP output events | All LGP clients |
| Power management | luna-power | `luna-power` (Volume V) | D-Bus: `org.lunaos.power.*` | Luna Performance Lab, AI advisor |
| Network connections | NetworkManager | `NetworkManager` | D-Bus: `org.freedesktop.NetworkManager` | luna-bar, lpkg, luna-ai-d |
| DNS resolution | System resolver | `/etc/resolv.conf` (DL-014) | POSIX `getaddrinfo()` | All processes |
| Time / clock | ntpd / chrony | `ntpd` | Kernel clock via `adjtimex()` | All processes (read via `gettimeofday()`) |
| Package installation | lpkg daemon | `lpkg` | D-Bus: `org.lunaos.lpkg.*` | All processes requesting installs |
| Package database | lpkg daemon | `lpkg` | SQLite at `/var/lib/lpkg/installed.db` | lpkg client, update checker |
| Audio routing | PipeWire | `pipewire` | PipeWire native protocol | All audio-producing apps |
| Process cgroup assignment | luna-init | `luna-init` | Internal — assigns at service start | Read via `/sys/fs/cgroup/` |
| Cgroup resource limits | luna-init | `luna-init` | Internal — configures at start. Performance Lab may write at runtime | Read via `/sys/fs/cgroup/` |
| AppArmor profiles | lpkg daemon | `lpkg` | Written to `/etc/apparmor.d/` at install time | Linux kernel enforces |
| AI mode (AMBIENT, FOCUS, etc.) | Presence Engine | `luna-ai-d` | D-Bus: `org.lunaos.luna.ModeChanged` signal | luna-island, luna-bar |
| AI conversation (LLM) | Inference Engine | `luna-ai-d` | D-Bus: `org.lunaos.luna.Chat()` | Any app via LUNA API |
| User memory (workflow, preferences) | Memory Engine | `luna-ai-d` | Exclusive file ownership: `~/.luna/memory/` | No other process reads |
| Accessibility tree | LunaGUI | `libLunaGUI` (in-process) | AT-SPI2 D-Bus interface (future) | Screen readers |
| LUNA Island surface | luna-island | `luna-island` | `LUNA_ISLAND` LGP surface type | Visual — no other process writes |
| LUNA expressions | Presence Engine | `luna-ai-d` | D-Bus signals → luna-island | luna-island renders |
| Firewall rules | luna-init | `luna-init` | Writes nftables config at boot. Runtime changes via `nft` | Luna Performance Lab (experimental) |
| System logs | luna-init (coordinates) | `luna-init` | Files in `/var/log/luna-init/` | All processes write to log API |
| Boot sequence | luna-init | `luna-init` | Internal — no external interface | — |
| Boot splash | luna-splash | `luna-splash` | Framebuffer directly | luna-init (kills it) |
| Compositor crash recovery | luna-init | `luna-init` | SIGCHLD detection, automatic restart | Compositor + all LGP clients |
| Screen lock | luna-lock | `luna-lock` | `LAYER_SYSTEM_MODAL` surface | User input |

---

## Ownership Detail: Key Capabilities

### Clipboard

**Owner: luna-shell**

The clipboard is a shared memory region that one application writes to and another reads from. On Mahina, the compositor (via luna-shell's request) holds the authoritative clipboard content.

```
State machine:
  EMPTY ──→ HAS_CONTENT (app writes to clipboard via LGP clipboard message)
         │
  HAS_CONTENT ──→ EMPTY (session ends or user clears)
              │
              ──→ HAS_CONTENT (new write replaces previous)
```

Per **DL-033**, the clipboard is implemented as the LGP protocol extension `lgp_ext_clipboard_v1`. Applications request clipboard write via `LGP_CLIPBOARD_SET` and read via `LGP_CLIPBOARD_GET`. The Permission Engine governs access. D-Bus clipboard services are not used.

---

### Notifications

**Owner: luna-notif**

`luna-notif` is the authoritative store of pending notifications. It implements `org.freedesktop.Notifications` D-Bus interface for compatibility.

```
State machine:
  EMPTY ──→ HAS_PENDING (Notify D-Bus call received)
         │
  HAS_PENDING ──→ DISPLAYED (luna-notif creates LGP surface)
              │
  DISPLAYED ──→ DISMISSED (user dismisses, or timeout)
             │
  DISMISSED ──→ EMPTY (surface destroyed)
             │
  DISPLAYED ──→ ACTION_ACTIVATED (user clicks notification action)
             │
  ACTION_ACTIVATED ──→ DISMISSED
```

Ownership boundary: luna-notif decides **when** to show a notification and **how long** it stays. luna-island may mirror notification summaries (showing a badge count) but does not own the notification state.

---

### Window Lifecycle

**Owner: LGP Compositor**

The compositor is the authoritative owner of every surface's lifecycle. Application code requests surface creation; the compositor grants or denies it. Application code requests surface destruction; the compositor executes it.

```
Window lifecycle state machine:

  [Client connects]
        │
        ▼
  CLIENT_CONNECTED
        │ LGP_CREATE_SURFACE
        ▼
  SURFACE_CREATED (not yet visible)
        │ LGP_COMMIT_BUFFER + LGP_SET_GEOMETRY
        ▼
  SURFACE_VISIBLE
        │
        ├──→ SURFACE_MINIMIZED (shell request)
        │         │ (shell restores)
        │         └──→ SURFACE_VISIBLE
        │
        ├──→ SURFACE_OCCLUDED (another surface on top — still alive)
        │         └──→ SURFACE_VISIBLE (top surface removed)
        │
        └──→ LGP_DESTROY_SURFACE
                  │
                  ▼
            SURFACE_DESTROYED
                  │
                  ▼
            [Client may disconnect or create new surface]
```

---

### Theme

**Two owners — split by concern:**

| Concern | Owner | What they own |
|---|---|---|
| Active theme TOML file | `luna-settings` | Writes `~/.luna/config/theme.toml` with the chosen theme name |
| Color resolution (semantic → hex) | `lgp-compositor` | Loads TOML, maintains Color Resolver, broadcasts `LGP_THEME_CHANGED` |

luna-settings owns the **decision** of which theme is active. The compositor owns the **application** of that theme to all surfaces. No other component reads `~/.luna/config/theme.toml` directly — they receive the resolved theme via LGP_THEME_CHANGED or `LunaTheme.current()`.

---

### Session

**Owner: luna-init**

Session state covers: which user is logged in, when the session started, and when it ends. luna-init supervises the entire session lifecycle.

```
Session state machine:

  BOOT ──→ SERVICES_UP (luna-init Stage 4 complete)
        │
        ▼
  GRAPHICS_UP (compositor ready — Stage 5 complete)
        │
        ▼
  SHELL_UP (luna-shell, luna-bar, luna-island running — Stage 6 complete)
        │
        ▼
  SESSION_ACTIVE (user interacting)
        │
        ├──→ LOCKED (screen lock — session active but input blocked)
        │         └──→ SESSION_ACTIVE (user unlocks)
        │
        └──→ SHUTDOWN_INITIATED (user requests shutdown / SIGTERM to luna-init)
                  │
                  ▼
            STAGE6_TEARDOWN (shell components stopped)
                  │
                  ▼
            STAGE4_TEARDOWN (system services stopped)
                  │
                  ▼
            KERNEL_SHUTDOWN
```

---

### Accessibility

**Owner: LunaGUI (in-process)**

The accessibility tree is maintained by LunaGUI inside each application process. There is no separate accessibility daemon in v1.

Per **DL-040**, Mahina exposes accessibility information via **AT-SPI2**. v1 implements full keyboard navigation in all LunaGUI widgets. v1.5 targets an AT-SPI2 D-Bus bridge allowing external screen readers to interrogate the widget tree.

---

### AI Mode and Expressions

**Owner: luna-ai-d (Presence Engine)**

```
LUNA mode state machine:

  AMBIENT ──→ DEVSHELL (terminal app focused + coding context detected)
           │
           ──→ FOCUS (single app focused, no context change for > 5 min)
           │
           ──→ STUDY (document reader / note app focused)
           │
           ──→ CREATIVE (creative app focused: image editor, DAW, etc.)
           │
           ──→ GAMING (game CANVAS_SURFACE active)
           │
  [any mode] ──→ AMBIENT (no focused app / idle > 10 min)
```

Only the Presence Engine writes mode state. luna-island and luna-bar are read-only consumers via D-Bus ModeChanged signal.

---

## Failure Mode Ownership

When a component fails, ownership of its recovery belongs to:

| Failed Component | Recovery Owner | Recovery Action |
|---|---|---|
| lgp-compositor | luna-init | Detect crash via SIGCHLD, restart compositor, signal Stage 6 clients to reconnect |
| luna-shell | luna-init | Restart luna-shell — compositor still running, desktop recovers |
| luna-island | luna-init | Restart luna-island — LUNA reappears |
| luna-ai-d | luna-init | Restart luna-ai-d — LUNA mode resets to AMBIENT |
| Ollama (LLM) | luna-ai-d | Restart Ollama subprocess — conversation API returns error until recovered |
| NetworkManager | luna-init | Restart NetworkManager — network reconnects |
| D-Bus daemon | luna-init | **Critical failure** — most services depend on D-Bus. luna-init attempts restart. If D-Bus cannot restart, initiate controlled shutdown. |
| lpkg daemon | luna-init | Restart lpkg daemon — any in-progress install is rolled back (DL-018 atomicity) |
| luna-notif | luna-init | Restart luna-notif — pending notifications are lost |
| luna-bar | luna-init | Restart luna-bar — status bar reappears within one boot cycle |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Power management.** `luna-power` is named as the owner but this process does not yet have a specification. Target: Volume V.

2. **luna-notif and luna-island badge relationship.** luna-island shows a badge count (a number of pending notifications). Does luna-notif send this count via D-Bus, or does luna-island read notification count directly? Ownership rule says luna-notif owns notification state — so luna-notif must publish the count via D-Bus. Confirm.

---

## AI Context

- If you are writing code for component X and you need to update a capability listed in the Ownership Table, check who owns it. If X is not the owner, X must request the change through the owner's interface.
- Never write directly to a capability you don't own. Writing to `/etc/nftables.conf` from `luna-ai-d` is a violation — nftables config is owned by luna-init.
- Never read from `~/.luna/memory/` from any process other than `luna-ai-d`. This is both an ownership rule and a Core Law II rule.
- The compositor owns window lifecycle. Applications cannot minimize, maximize, or close other applications' windows. They can only request these operations for their own surfaces.
- luna-notif owns notification state. luna-island may mirror a badge count but must receive it from luna-notif, not by reading notification state directly.

---

*Document: `Volume II / 13_component_ownership.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Addresses: BLOCKER 3 (IPC ownership gaps)*
*Cross-references: 08_ipc.md, 03_compositor.md, 04_init_system.md, non_negotiables.md, decision_log.md*
