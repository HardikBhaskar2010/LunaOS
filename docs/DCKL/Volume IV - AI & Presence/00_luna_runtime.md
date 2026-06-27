# Mahina — LUNA Runtime
**Volume IV · Chapter 0**
**Classification:** Core Architecture — AI System Entry Point
**Status:** Canonical · This is the single document that explains what LUNA is, what process owns her, and how all her components relate

---

## Purpose

This document answers the question the Principal Engineer asked: **exactly what process owns LUNA?**

It defines the LUNA Runtime — the complete set of processes, components, and interfaces that together constitute LUNA as an operating system entity. It is the entry point for Volume IV and the document every other Volume IV chapter depends on.

Without this document, "LUNA" is an ambiguous term used to mean luna-island, luna-ai-d, the Presence Engine, the Inference Engine, and the OS brand simultaneously. This document ends that ambiguity.

---

## Overview

LUNA is not a single process. LUNA is a **runtime** — a coordinated set of components that together produce the experience of a coherent, present AI entity. Understanding LUNA means understanding which component does what.

```
┌─────────────────────────────────────────────────────────────────┐
│                    THE LUNA RUNTIME                              │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │              luna-ai-d (The AI Process)                  │     │
│  │                                                           │     │
│  │   ┌───────────────────┐   ┌───────────────────────────┐  │     │
│  │   │  Presence Engine  │   │  LLM Inference Engine      │  │     │
│  │   │                   │   │                             │  │     │
│  │   │  Always running.  │   │  Lazy — starts on first    │  │     │
│  │   │  Lightweight.     │   │  LLM request.              │  │     │
│  │   │  Watches context. │   │  Owns Ollama subprocess.   │  │     │
│  │   │  Drives mode.     │   │  Handles conversation.     │  │     │
│  │   │  Drives Island.   │   │  Returns to idle when not  │  │     │
│  │   │  Drives memory.   │   │  in use (DL-021).          │  │     │
│  │   └────────┬──────────┘   └──────────────┬────────────┘  │     │
│  │            │                              │                │     │
│  │            └──────────────┬───────────────┘                │     │
│  │                           │ D-Bus signals + methods         │     │
│  └───────────────────────────┼────────────────────────────────┘     │
│                              │                                        │
│  ┌───────────────────────────▼────────────────────────────────┐     │
│  │              luna-island (The Visual Body)                   │     │
│  │                                                               │     │
│  │  A separate process. An LGP client.                           │     │
│  │  Owns the LUNA_ISLAND compositor surface.                     │     │
│  │  Renders LUNA's visual presence on screen.                    │     │
│  │  Subscribes to D-Bus signals from luna-ai-d.                  │     │
│  │  luna-island does NOT make AI decisions. It renders them.     │     │
│  └───────────────────────────────────────────────────────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────┘
```

**The definitive answer:** `luna-ai-d` owns LUNA's intelligence. `luna-island` owns LUNA's appearance. They communicate through D-Bus. Neither can function fully without the other — but they have separate lifecycles and can restart independently.

---

## Component Definitions

### luna-ai-d — The AI Process

`luna-ai-d` is a single long-lived daemon process. It is the authoritative owner of:
- LUNA's current mode (AMBIENT, DEVSHELL, FOCUS, STUDY, CREATIVE, GAMING)
- LUNA's memory of the user (`~/.luna/memory/`)
- LUNA's context (what the user is doing right now)
- The LLM conversation API
- The Presence Engine and Inference Engine components (both run inside this process)

`luna-ai-d` starts at luna-init Stage 6. It runs for the entire session. It shuts down gracefully at Session teardown (summarizes memory before exit).

**Process characteristics:**
- Language: TODO — Unconfirmed for v1 (C vs Python). Rust migration planned for v2 (analogous to DL-007). Needs DL entry.
- Cgroup: `luna-ai.slice`
- Runs as: the logged-in user (not root)
- Persistent state: `~/.luna/memory/` and `~/.luna/config/`
- IPC: D-Bus (publishes services) + LGP socket (connects as LGP client for luna-island signaling)

---

### Presence Engine — The Observer

The Presence Engine is a **component** of `luna-ai-d`, not a separate process.

It is responsible for:
- Reading `~/.luna/config/observe.toml` to know which apps to observe
- Monitoring application focus events (received from the compositor via LGP)
- Detecting the user's current context from observed application + file activity
- Computing the current LUNA mode
- Writing observations to `~/.luna/memory/workflow.db`
- Triggering expression decisions (which visual expression LUNA shows) and publishing them via D-Bus

The Presence Engine is **always running** inside `luna-ai-d`. It is the lightweight, always-on component. It uses under 100 MB RAM. It does not use the LLM. It does not use Ollama.

**The Presence Engine is what makes LUNA "present" even when she isn't thinking.**

---

### LLM Inference Engine — The Thinker

The LLM Inference Engine is a **component** of `luna-ai-d`, not a separate process.

It is responsible for:
- Starting Ollama as a subprocess on first LLM demand (lazy start — DL-021)
- Forwarding user conversation requests to Ollama
- Returning LLM responses via D-Bus streaming signals
- Stopping Ollama when idle for more than a configurable timeout
- Managing the Ollama model lifecycle (which model is loaded)

The Inference Engine is **dormant at boot**. It activates only when the user explicitly requests a LUNA conversation or when the Presence Engine determines that a conversational response is warranted at Expression Layer priority 6 or 7.

**The Inference Engine is what makes LUNA "think" when she needs to.**

---

### luna-island — The Visual Body

`luna-island` is a **separate process** from `luna-ai-d`. It is a graphical LGP client.

It is responsible for:
- Holding the `LUNA_ISLAND` LGP surface (the visual element on screen)
- Rendering LUNA's current expression state (mode indicator, animations, notifications)
- Listening to D-Bus signals from `luna-ai-d` and translating them into LGP animations
- Managing the Luna Island component as specified in `identity.md`

`luna-island` **does not make decisions**. It renders decisions that `luna-ai-d` has made. If `luna-ai-d` says "switch to DEVSHELL mode," luna-island plays the appropriate animation and updates the visual state.

`luna-island` **does not own AI state**. It holds no memory, no mode state, no conversation history. If it crashes and restarts, it requests the current mode from `luna-ai-d` on reconnect and renders accordingly.

---

## LUNA Runtime State Machine

```
LUNA Runtime overall state:

  OFFLINE (luna-ai-d not running)
      │
      │ luna-init Stage 6 starts luna-ai-d
      ▼
  INITIALIZING
      │ Presence Engine starts, reads config
      │ observe.toml loaded
      │ workflow.db opened
      ▼
  AMBIENT (default mode — no specific context detected)
      │
      ├──→ DEVSHELL (terminal/IDE focused + code context)
      │         │
      │         └──→ AMBIENT (idle > 10min or context changes)
      │
      ├──→ FOCUS (single app, concentrated work)
      │         └──→ AMBIENT
      │
      ├──→ STUDY (reading/notes context)
      │         └──→ AMBIENT
      │
      ├──→ CREATIVE (creative app context)
      │         └──→ AMBIENT
      │
      ├──→ GAMING (CANVAS_SURFACE app active)
      │         └──→ AMBIENT (game exits)
      │
      └──[any mode]──→ CONVERSING (user initiates LUNA conversation)
                            │ Inference Engine activates
                            │ Ollama starts (if not already running)
                            │
                            ├──→ THINKING (LLM processing request)
                            │         │
                            │         └──→ RESPONDING (streaming tokens to user)
                            │                   │
                            │                   └──→ [previous mode] (response complete)
                            │
                            └──→ [previous mode] (user ends conversation)
```

---

## LUNA Runtime Interfaces

### luna-ai-d D-Bus Services

**Service name:** `org.mahina.luna`

| Method / Signal | Type | Description |
|---|---|---|
| `GetMode()` → `string` | Method | Returns current LUNA mode string |
| `GetContext()` → `dict` | Method | Returns current context (active app, file type, time in session) |
| `Chat(prompt: string)` → `string` | Method | Synchronous LLM conversation (short timeout) |
| `ChatStream(prompt: string)` | Method | Initiates streaming LLM conversation |
| `ModeChanged(new_mode: string)` | Signal | Emitted when LUNA mode changes |
| `ExpressionChanged(expression: dict)` | Signal | Emitted when Presence Engine decides a new visual expression |
| `TokenReceived(token: string, is_final: bool)` | Signal | Streaming LLM response token |
| `MemoryCleared()` | Signal | Emitted after `luna memory --clear` completes |

### luna-island LGP Connection

luna-island connects to the LGP compositor as a standard LGP client. It requests the `LUNA_ISLAND` surface type. The compositor enforces that only luna-island may create this surface type.

luna-island receives:
- `LGP_FRAME_CALLBACK` — to animate its surface
- `LGP_INPUT_EVENT` — if the user clicks/taps the Island

luna-island sends:
- `LGP_COMMIT_BUFFER` — its rendered frame
- `LGP_SEND_MOTION` — when animating expression changes
- `LGP_SET_SEMANTIC_COLOR` — when updating the Island's state color

---

## LUNA Runtime Failure Modes

```
Failure state diagram:

  NORMAL OPERATION
      │
      ├──→ luna-ai-d CRASH
      │         │ luna-init detects via SIGCHLD
      │         │ Restart attempt #1
      │         ├──→ RESTART SUCCEEDS: mode resets to AMBIENT
      │         │                       luna-island reconnects via D-Bus
      │         │                       memory is intact (disk-persisted)
      │         │
      │         └──→ RESTART FAILS (twice in 30s):
      │                   luna-island shows LUNA_VOID mode (gray indicator)
      │                   AI features unavailable until manual intervention
      │
      ├──→ luna-island CRASH
      │         │ luna-init detects via SIGCHLD
      │         │ Restart luna-island
      │         │ luna-island reconnects to compositor
      │         │ luna-island requests current mode from luna-ai-d via D-Bus
      │         └──→ LUNA Island reappears, showing correct current mode
      │
      ├──→ Ollama CRASH (LLM Inference Engine subprocess)
      │         │ luna-ai-d detects via SIGCHLD
      │         │ Inference Engine marks Ollama as unavailable
      │         │ Restart Ollama subprocess
      │         │ In-progress conversation: returns error to caller
      │         └──→ Ollama restarted, next conversation request works
      │
      └──→ Ollama OOM-killed by kernel
                │ Same recovery path as Ollama CRASH above
                │ But: luna-ai-d logs at ERROR level
                │ luna-island briefly shows LUNA Amber (OOM warning)
                └──→ Ollama restarted — may need to reload model from disk
```

---

## LUNA Runtime — Technical Details

### Boot Sequence (Detailed)

```
luna-init Stage 6:

  1. Start luna-ai-d
       a. Presence Engine initializes
       b. Read ~/.luna/config/observe.toml
       c. Open ~/.luna/memory/workflow.db
       d. Load persistent_summary.enc (decrypt if exists)
       e. Connect to compositor LGP socket (for context signals)
       f. Register D-Bus services: org.mahina.luna
       g. Signal luna-init: LUNA_PRESENCE_READY

  2. Start luna-island
       a. Connect to compositor LGP socket
       b. Create LUNA_ISLAND surface
       c. Query luna-ai-d: GetMode() → AMBIENT (first boot) or last mode
       d. Render initial Island state (AMBIENT appearance)
       e. Subscribe to luna-ai-d D-Bus: ModeChanged, ExpressionChanged
       f. Signal luna-init: LUNA_ISLAND_READY

  3. luna-init Stage 6 complete → SESSION_ACTIVE
```

### Shutdown Sequence (Detailed)

```
luna-init shutdown → Stage 6 teardown:

  1. SIGTERM → luna-island
       a. luna-island plays collapse animation (LunaMotion.Collapse)
       b. Destroys LUNA_ISLAND LGP surface
       c. Exits cleanly

  2. SIGTERM → luna-ai-d
       a. Presence Engine pauses observation
       b. Memory Engine: summarization pass over workflow.db, preference.db
       c. Write encrypted persistent_summary.enc to disk
       d. Send signal to Ollama subprocess: SIGTERM (Ollama exits)
       e. luna-ai-d exits cleanly

  (luna-init proceeds with Stage 4 teardown)
```

### Memory Isolation

```
Memory access rules for LUNA Runtime:

  luna-ai-d:
    ✅ Reads and writes ~/.luna/memory/
    ✅ Reads ~/.luna/config/
    ❌ Cannot access other users' ~/.luna/
    ❌ Cannot access /etc/ (except reading /etc/luna/observe.toml which is world-readable)
    ❌ Cannot write to /var/lib/ (no root)

  luna-island:
    ✅ Reads its own configuration from ~/.luna/config/island.toml
    ❌ Cannot read ~/.luna/memory/ — only luna-ai-d reads memory
    ❌ Cannot write to ~/.luna/memory/
```

---

## Decoupling Rule

luna-ai-d and luna-island are deliberately separate processes. This means:

- luna-island can crash without killing LUNA's AI state
- luna-ai-d can crash without destroying the graphical shell
- They can be restarted independently
- They can be updated independently (luna-island is a graphical update; luna-ai-d is an AI update)

**Never merge luna-island and luna-ai-d into a single process.** The decoupling is intentional and must be maintained.

---

## Current Decisions

| Decision | Status | Source |
|---|---|---|
| luna-ai-d is the owner of LUNA's intelligence | Accepted | DL-021, this document |
| luna-island is a separate process (LGP client) | Accepted | DL-004R, this document |
| Presence Engine is a component of luna-ai-d | Accepted | DL-021 |
| LLM Inference Engine is a component of luna-ai-d | Accepted | DL-021 |
| Inference Engine is lazy-loaded on first demand | Accepted | DL-021 |
| luna-ai-d communicates with luna-island via D-Bus | Accepted | This document |
| luna-ai-d runs as the logged-in user (not root) | Accepted | Volume II / 06_memory.md |
| Memory encryption at rest is a v1 requirement | Accepted | DL-023 |
| Live2D model for luna-island | Provisional — v1.5 | Volume III / 05_animation_engine.md |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Live2D integration.** v1 luna-island uses static LGP surface transforms. Live2D would replace the rendering model entirely. Must be evaluated as a v1.5 project. License review required.

2. **Mode detection thresholds.** The timeouts in the mode state machine (idle > 10 min → AMBIENT, single app focused > 5 min → FOCUS) are estimates. Must be validated against real user workflows.

3. **Multi-user sessions.** Does each user get their own `luna-ai-d` instance? Presumably yes — each user has their own `~/.luna/`. Must be confirmed.

---

## AI Context

- **`luna-ai-d` owns LUNA's intelligence.** If you are writing AI-decision code, it belongs in `luna-ai-d`.
- **`luna-island` owns LUNA's appearance.** If you are writing rendering code for the Island surface, it belongs in `luna-island`.
- These two processes communicate via D-Bus only. Never link them. Never have one call the other's C functions directly.
- When implementing a new LUNA behavior: ask "is this a decision (luna-ai-d) or a rendering (luna-island)?" The answer tells you which process it belongs in.
- The Presence Engine is always on. The Inference Engine is dormant at boot. Never start Ollama at luna-ai-d startup — it starts lazily on first demand (DL-021).
- `~/.luna/memory/` is exclusive to `luna-ai-d`. No other process reads or writes it. Not luna-island. Not luna-shell. Not any application. This is Core Law II.
- LUNA's mode is published via D-Bus. Any component that needs to know the current mode calls `org.mahina.luna.GetMode()` or subscribes to `ModeChanged`. It does not read LUNA state from files.

---

*Document: `Volume IV / 00_luna_runtime.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Addresses: BLOCKER 4 (LUNA process ownership unclear)*
*Classification: Canonical entry point for Volume IV*
*Depends on: non_negotiables.md, decision_log.md (DL-021, DL-023), identity.md, luna_personality.md, 06_memory.md, 08_ipc.md*
*Informs: All Volume IV documents (context engine, memory engine, conversation, inference)*
