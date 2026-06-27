# LunaOS — Presence Engine
**Volume IV · Chapter 1**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · This is the authoritative specification for the Presence Engine component of luna-ai-d

---

## Purpose

This document specifies the **Presence Engine** — the component of `luna-ai-d` that is always running, always observing, and always deciding how LUNA should present herself to the user.

The Presence Engine is what makes LunaOS feel **alive without being intrusive**. It observes the user's context, determines which mode LUNA should be in, and produces expression decisions that luna-island renders. It does all of this without the LLM — the Presence Engine is lightweight, fast, and always on.

Without this document, there is no definition of what the Presence Engine actually does moment to moment, what it observes, how it makes decisions, or what outputs it produces.

---

## Overview

```
Presence Engine — position in the system:

  ┌─────────────────────────────────────────────────────────────┐
  │                   luna-ai-d process                          │
  │                                                               │
  │   ┌─────────────────────────────────────────────────────┐   │
  │   │                 PRESENCE ENGINE                      │   │
  │   │   (this document)                                    │   │
  │   │                                                       │   │
  │   │  Inputs:                                              │   │
  │   │   ← LGP focus events (via compositor socket)         │   │
  │   │   ← D-Bus: luna-notif notification count            │   │
  │   │   ← File: ~/.luna/config/observe.toml               │   │
  │   │   ← File: ~/.luna/memory/workflow.db (read)         │   │
  │   │                                                       │   │
  │   │  Processing:                                          │   │
  │   │   • Context classifier (rule-based, no LLM)          │   │
  │   │   • Mode state machine                               │   │
  │   │   • Expression selector                              │   │
  │   │                                                       │   │
  │   │  Outputs:                                             │   │
  │   │   → D-Bus: ModeChanged signal                        │   │
  │   │   → D-Bus: ExpressionChanged signal                  │   │
  │   │   → File: ~/.luna/memory/workflow.db (write)         │   │
  │   └─────────────────────────────────────────────────────┘   │
  │                                                               │
  │   ┌──────────────┐    ← separate component                   │
  │   │ LLM Inference│    ← NOT used by Presence Engine          │
  │   └──────────────┘                                           │
  └─────────────────────────────────────────────────────────────┘
```

**Critical rule:** The Presence Engine **never calls the LLM**. It uses only:
- Rule-based logic
- Lightweight heuristics
- The user's workflow history (read from `workflow.db`)
- The active application and file context from the compositor

If a decision requires language understanding or reasoning, it is deferred to the Inference Engine. The Presence Engine is only responsible for fast, always-on context awareness.

---

## Architecture

### Internal Structure

```
Presence Engine components:

  ┌──────────────────────────────────────────────────┐
  │              Context Observer                     │
  │  Receives: LGP focus events                       │
  │  Produces: raw context structs                    │
  │  (what app is focused, what file is open)        │
  └─────────────────────┬────────────────────────────┘
                        │
  ┌─────────────────────▼────────────────────────────┐
  │              Context Classifier                   │
  │  Input:  raw context struct                       │
  │  Logic:  rule-based matching against observe.toml │
  │  Output: classified context (CODING, READING,     │
  │          CREATIVE, GAMING, IDLE, etc.)            │
  └─────────────────────┬────────────────────────────┘
                        │
  ┌─────────────────────▼────────────────────────────┐
  │              Mode State Machine                   │
  │  Input:  classified context + time in state       │
  │  Logic:  transition rules + hysteresis timers     │
  │  Output: current LUNA mode (AMBIENT, DEVSHELL,   │
  │          FOCUS, STUDY, CREATIVE, GAMING)          │
  └──────────┬──────────────────────┬────────────────┘
             │                      │
  ┌──────────▼──────────┐  ┌────────▼─────────────────┐
  │  Expression Selector│  │  Workflow Recorder        │
  │  Selects expression │  │  Writes session data      │
  │  type for luna-island│  │  to workflow.db           │
  │  Emits ExpressionChg│  │  (app, file, duration)    │
  └──────────┬──────────┘  └──────────────────────────┘
             │
  ┌──────────▼──────────┐
  │  D-Bus Publisher    │
  │  Emits ModeChanged  │
  │  Emits ExpressionChg│
  └─────────────────────┘
```

---

## Context Observer

The Context Observer listens for events that indicate what the user is doing:

### Event Sources

**1. LGP Focus Events**
The compositor delivers `LGP_FOCUS_CHANGED` events when the keyboard-focused surface changes. The Context Observer receives:
- `surface_id` of the newly focused surface
- The application name (from the surface's `client_name` field in `LGP_HELLO`)
- The surface type (`APPLICATION_WINDOW`, `CANVAS_SURFACE`, etc.)

**2. Active File Path (via D-Bus, opt-in)**
If the focused application declares `observe_active_file = true` in `observe.toml`, it may publish its active file path to D-Bus at `org.lunaos.context.ActiveFile`. Applications that do not publish this are observed at the application level only (no file-level context).

**3. Idle Detection**
If no focus change events arrive for a configurable `idle_timeout` (default: 5 minutes), the Context Observer emits an IDLE context. The Mode State Machine responds to IDLE by transitioning toward AMBIENT.

### Raw Context Struct

```c
typedef struct luna_context {
    char     app_name[128];       // e.g. "luna-terminal", "code", "firefox"
    char     app_class[64];       // classify: TERMINAL, BROWSER, EDITOR, GAME, etc.
    char     active_file[512];    // e.g. "/home/user/projects/lunagui/layout.c"
    char     file_extension[16];  // e.g. ".c", ".md", ".py"
    uint32_t seconds_in_app;      // how long this app has been focused
    bool     is_fullscreen;       // CANVAS_SURFACE or fullscreen APPLICATION_WINDOW
    bool     is_idle;             // no input events for idle_timeout seconds
    uint64_t timestamp;           // Unix timestamp of this context snapshot
} luna_context_t;
```

---

## Context Classifier

The Context Classifier takes the raw context struct and produces a **classified context** using rules defined in `observe.toml`.

### observe.toml Format

```toml
# ~/.luna/config/observe.toml
# Installed by lpkg at application install time
# User may edit to add custom rules

[observe]
idle_timeout_seconds = 300     # 5 minutes before IDLE context
min_app_focus_seconds = 10     # ignore focus events < 10s (accidental switches)

# Application classification rules
[[observe.app_rules]]
pattern = "luna-terminal"       # exact app name match
context = "CODING"
reason  = "Terminal is a coding context by default"

[[observe.app_rules]]
pattern = "code"
context = "CODING"
reason  = "VS Code / similar editor"

[[observe.app_rules]]
pattern = "*"
file_extension = [".c", ".h", ".cpp", ".rs", ".py", ".go", ".lua"]
context = "CODING"
reason  = "Any app with a code file extension"

[[observe.app_rules]]
pattern = "*"
file_extension = [".md", ".txt", ".pdf", ".epub"]
context = "READING"
reason  = "Document / reading context"

[[observe.app_rules]]
pattern = "blender"
context = "CREATIVE"

[[observe.app_rules]]
pattern = "gimp"
context = "CREATIVE"

[[observe.app_rules]]
pattern = "*"
surface_type = "CANVAS_SURFACE"
context = "GAMING"
reason  = "Canvas surface apps are games or performance applications"

# Fallback
[[observe.app_rules]]
pattern = "*"
context = "GENERAL"
reason  = "Default for unclassified applications"
```

### Classification Output

| Classified Context | Example Triggers | Maps to Mode |
|---|---|---|
| `CODING` | Terminal, editor, code file extension | DEVSHELL |
| `READING` | PDF, markdown, browser on long article | STUDY |
| `CREATIVE` | Blender, GIMP, DAW application | CREATIVE |
| `GAMING` | Any CANVAS_SURFACE application | GAMING |
| `BROWSING` | Browser, not on a reading-classified URL | FOCUS |
| `GENERAL` | Most standard applications | FOCUS |
| `IDLE` | No input for `idle_timeout` seconds | AMBIENT |
| `MULTI_TASKING` | Rapid app switching (> 3 apps in 60s) | FOCUS |

---

## Mode State Machine

The Mode State Machine converts classified contexts into LUNA modes. It applies **hysteresis** — a delay before transitioning — to prevent mode flickering when the user briefly switches apps.

### Modes

| Mode | Trigger | Luna Island Color | Expression |
|---|---|---|---|
| `AMBIENT` | Idle, no specific context | LUNA_WHITE | PULSE_GENTLE |
| `DEVSHELL` | CODING context ≥ 30s | LUNA_GREEN | FOCUS_RING |
| `FOCUS` | GENERAL or BROWSING ≥ 5min | LUNA_AMBER | PULSE_GENTLE (dimmer) |
| `STUDY` | READING context ≥ 30s | LUNA_WHITE | DIM |
| `CREATIVE` | CREATIVE context ≥ 30s | LUNA_GREEN | SHIMMER |
| `GAMING` | GAMING context (immediate) | LUNA_VOID | VOID (minimal) |

### State Transition Rules

```
Mode State Machine:

  Initial state: AMBIENT

  AMBIENT:
    → DEVSHELL  if CODING context is active for ≥ 30 seconds
    → FOCUS     if GENERAL context is active for ≥ 5 minutes
    → STUDY     if READING context is active for ≥ 30 seconds
    → CREATIVE  if CREATIVE context is active for ≥ 30 seconds
    → GAMING    if GAMING context is active (immediate, no hysteresis)

  DEVSHELL:
    → AMBIENT   if IDLE for ≥ 5 minutes
    → GAMING    if GAMING context (immediate)
    → STUDY     if READING context for ≥ 60 seconds (less sensitive in DEVSHELL)
    → CREATIVE  if CREATIVE context for ≥ 60 seconds

  FOCUS:
    → AMBIENT   if IDLE for ≥ 5 minutes
    → DEVSHELL  if CODING context for ≥ 30 seconds
    → GAMING    if GAMING context (immediate)
    → STUDY     if READING context for ≥ 30 seconds

  STUDY:
    → AMBIENT   if IDLE for ≥ 5 minutes
    → DEVSHELL  if CODING context for ≥ 30 seconds
    → GAMING    if GAMING context (immediate)

  CREATIVE:
    → AMBIENT   if IDLE for ≥ 5 minutes
    → GAMING    if GAMING context (immediate)

  GAMING:
    → AMBIENT   immediately when GAMING context ends
                (user exits fullscreen / CANVAS_SURFACE destroyed)
```

**Hysteresis implementation:**
```c
// Hysteresis timer per mode transition
// Prevents flickering when user briefly opens a terminal then switches back
typedef struct mode_hysteresis {
    luna_mode_t   target_mode;
    uint32_t      required_seconds;
    uint64_t      start_timestamp;  // when this context started accumulating
} mode_hysteresis_t;

// Only emit ModeChanged when:
//   current_time - start_timestamp >= required_seconds
// Reset the timer if the classified context changes before threshold is reached
```

---

## Expression Selector

After every mode transition or significant context change, the Expression Selector determines which expression luna-island should render.

### Expression Selection Logic

```c
luna_expression_t select_expression(luna_mode_t mode, luna_context_t ctx) {
    // Priority 7: System critical — checked first, bypasses all other logic
    if (system_oom_detected())
        return (luna_expression_t){ .type = FLASH, .color = LUNA_PINK, .duration_ms = 2000 };

    // Priority 6: LUNA alert (pending response from user, new urgent notification)
    if (has_urgent_notification())
        return (luna_expression_t){ .type = PULSE_ALERT, .color = LUNA_AMBER, .duration_ms = 0 };

    // Priority 5: User is actively using the Island — handled by luna-island, not here

    // Priority 4: AI response in progress (Inference Engine signaled)
    if (inference_engine_active())
        return (luna_expression_t){ .type = GLOW, .color = LUNA_GREEN, .duration_ms = 0 };

    // Priority 3: Mode-based expression (context change)
    switch (mode) {
        case LUNA_MODE_AMBIENT:    return (luna_expression_t){ .type = PULSE_GENTLE, .color = LUNA_WHITE,  .duration_ms = 0 };
        case LUNA_MODE_DEVSHELL:   return (luna_expression_t){ .type = FOCUS_RING,   .color = LUNA_GREEN,  .duration_ms = 0 };
        case LUNA_MODE_FOCUS:      return (luna_expression_t){ .type = PULSE_GENTLE, .color = LUNA_AMBER,  .duration_ms = 0 };
        case LUNA_MODE_STUDY:      return (luna_expression_t){ .type = DIM,          .color = LUNA_WHITE,  .duration_ms = 0 };
        case LUNA_MODE_CREATIVE:   return (luna_expression_t){ .type = SHIMMER,      .color = LUNA_GREEN,  .duration_ms = 0 };
        case LUNA_MODE_GAMING:     return (luna_expression_t){ .type = VOID,         .color = LUNA_VOID,   .duration_ms = 0 };
    }
}
```

### Expression Emission Rules

1. Expressions with `duration_ms = 0` are **permanent** until the next `ExpressionChanged` signal.
2. Expressions with a `duration_ms > 0` are **temporary** — they expire after the duration and luna-island reverts to the previous expression.
3. The Presence Engine never emits the same expression twice in a row (deduplication).
4. Mode changes always emit a new expression, even if the expression type is the same (color may differ).

---

## Workflow Recorder

The Workflow Recorder writes session data to `~/.luna/memory/workflow.db` (SQLite).

### Schema

```sql
-- workflow.db schema

CREATE TABLE sessions (
    session_id   TEXT PRIMARY KEY,
    start_time   INTEGER NOT NULL,  -- Unix timestamp
    end_time     INTEGER,           -- NULL while session is active
    summary      TEXT               -- filled by Memory Engine at session end
);

CREATE TABLE app_events (
    event_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id   TEXT NOT NULL REFERENCES sessions(session_id),
    timestamp    INTEGER NOT NULL,
    app_name     TEXT NOT NULL,
    file_path    TEXT,              -- NULL if app did not publish file
    classified_context TEXT NOT NULL, -- CODING, READING, etc.
    duration_seconds INTEGER        -- how long this focus event lasted
);

CREATE TABLE mode_events (
    event_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id   TEXT NOT NULL,
    timestamp    INTEGER NOT NULL,
    from_mode    TEXT NOT NULL,
    to_mode      TEXT NOT NULL,
    trigger      TEXT NOT NULL      -- what caused the transition
);
```

### Write Rules

- A new `app_event` row is written when the user focuses a different app (or when the session ends)
- A new `mode_event` row is written on every mode transition
- The current session's `end_time` is updated to `NULL` (open) during the session and set to the actual time at shutdown
- workflow.db is opened in WAL mode for safe concurrent access (Memory Engine reads it during summarization)

---

## Startup Sequence

```
Presence Engine startup (part of luna-ai-d startup):

  1. Read ~/.luna/config/observe.toml
     → Parse app rules
     → Set idle_timeout and min_app_focus_seconds
     → Log: "Observation rules loaded: N rules"

  2. Open ~/.luna/memory/workflow.db
     → Create tables if not exist (first boot)
     → Insert new session row with start_time = now
     → Log: "Workflow database opened, session started"

  3. Connect to LGP compositor socket
     → Register as an observation-only LGP client (no surface)
     → Subscribe to LGP_FOCUS_CHANGED events
     → Log: "LGP context observer connected"

  4. Connect to D-Bus
     → Subscribe to luna-notif notification count changes
     → Log: "D-Bus subscriptions active"

  5. Set initial context: IDLE (no focus info yet)
     → Mode: AMBIENT
     → Emit: ModeChanged(AMBIENT)
     → Emit: ExpressionChanged(PULSE_GENTLE, LUNA_WHITE)
     → Log: "Presence Engine ready — initial mode: AMBIENT"

  6. Signal luna-ai-d: PRESENCE_ENGINE_READY
     → luna-ai-d may now signal luna-init: LUNA_PRESENCE_READY
```

---

## Shutdown Sequence

```
Presence Engine shutdown (part of luna-ai-d shutdown):

  1. Pause observation (stop processing new LGP events)
  2. Write final app_event row for current focus
  3. Write final mode_event row
  4. Update session end_time = now
  5. Signal Memory Engine: SESSION_ENDED (triggers summarization)
  6. Close workflow.db
  7. Disconnect from compositor LGP socket
  8. Log: "Presence Engine shutdown complete"
```

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| CPU usage (idle, no events) | 0% | < 0.1% |
| CPU usage (processing focus event) | < 1ms | 5ms |
| RAM usage (process contribution) | < 20 MB | 40 MB |
| Mode decision latency | < 5ms | 20ms |
| D-Bus signal emit latency | < 10ms | 50ms |
| workflow.db write latency | < 2ms | 10ms |

The Presence Engine is the "always on" component. Its performance budget must be treated with the same discipline as a kernel interrupt handler. Every additional feature must be justified against the CPU cost.

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Presence Engine is a component of luna-ai-d, not a separate process | DL-042, Volume IV/00 | ✅ Accepted |
| Presence Engine never calls the LLM | AP-002, Volume IV/00 | ✅ Accepted |
| Context observation is opt-in via observe.toml | DL-022 | ✅ Accepted |
| Observation rules are per-application, installed by lpkg | DL-022 | ✅ Accepted |
| Mode hysteresis: 30s default for most transitions | This document | ✅ Accepted |
| GAMING mode is immediate (no hysteresis) | This document | ✅ Accepted |
| workflow.db is SQLite in WAL mode | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **File path observation privacy.** observe.toml allows apps to opt in to publishing the active file path. Should file paths be stored raw in workflow.db, or should they be anonymized (just the extension, not the full path)? Must be a Decision Log entry — this is a privacy decision.

2. **Idle timeout configurability.** The 5-minute idle timeout is a default. Should users be able to change it in luna-settings? Likely yes.

3. **Mode transition notifications.** Should LUNA proactively say something when transitioning to certain modes (e.g., "Switching to DEVSHELL — I see you're in lunagui/layout.c")? This crosses into Inference Engine territory. Must be a Decision Log entry.

4. **Multi-monitor focus.** If the user has two monitors with different apps focused on each, what context does the Presence Engine use? The primary display? The most recently focused surface? Must be resolved before multi-display support.

5. **Browser context classification.** A browser showing a code file (e.g., GitHub) might deserve CODING context, but a browser showing social media deserves GENERAL. URL-based classification requires browser integration. Is URL observation permitted? Must be a Decision Log entry.

---

## AI Context

- The Presence Engine is the **observer**. It watches. It does not speak, generate, or reason. All of those functions belong to the Inference Engine.
- Every operation in the Presence Engine must complete in < 5ms. If a proposed feature cannot complete in 5ms, it does not belong here.
- The mode state machine's hysteresis is intentional and must not be removed. Users switch apps frequently. Without hysteresis, LUNA's mode would change dozens of times per minute, making the Island visually noisy.
- `observe.toml` is the user's control over what the Presence Engine watches. Never observe something not listed in observe.toml. This is Core Law II (Privacy First).
- `workflow.db` is owned exclusively by luna-ai-d. No other process reads it. The Memory Engine (Volume IV/04) reads it as part of the summarization process — but only within the luna-ai-d process boundary.
- The Presence Engine starts before the Inference Engine and runs even when the LLM is not loaded. LUNA is present even without an AI model.

---

*Document: `Volume IV / 01_presence_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, non_negotiables.md, DL-022, AP-002*
*Informs: Volume IV/03_context_engine.md, Volume IV/04_memory_engine.md, Volume III/07_luna_island.md*
