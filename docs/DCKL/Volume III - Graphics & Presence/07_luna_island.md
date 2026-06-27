# LunaOS — Luna Island
**Volume III · Chapter 7**
**Classification:** Core Architecture — Graphics & Presence
**Status:** Canonical · This document defines the Luna Island surface specification

---

## Purpose

Luna Island is the **physical location of LUNA's presence on screen**. It is the one graphical element on the LunaOS desktop that LUNA owns and inhabits. It is not a widget, not a notification banner, not a status icon. It is the rendered body of LUNA as a presence.

This document specifies:
- What Luna Island is and what it renders
- Its three visual states and transitions
- Its surface architecture as an LGP client
- How it receives instructions from `luna-ai-d`
- Its interaction model (DL-034: hybrid short-click / long-press)
- Its motion choreography and expression system
- Failure modes and recovery

Without this document, implementors have no authoritative definition of what luna-island should render or how it should behave. This is that definition.

---

## Overview

```
Luna Island sits at the intersection of three systems:

  ┌──────────────────────────────────────────────────────┐
  │                   LUNA RUNTIME                        │
  │   luna-ai-d                                           │
  │   ├── Presence Engine  →  ModeChanged signal          │
  │   └── Inference Engine →  ExpressionChanged signal    │
  └────────────────────────┬─────────────────────────────┘
                           │ D-Bus
  ┌────────────────────────▼─────────────────────────────┐
  │               luna-island (this document)             │
  │                                                        │
  │   Receives:  D-Bus signals from luna-ai-d             │
  │   Renders:   LUNA_ISLAND LGP surface                  │
  │   Produces:  Motion-choreographed visual presence     │
  └────────────────────────┬─────────────────────────────┘
                           │ LGP protocol
  ┌────────────────────────▼─────────────────────────────┐
  │               lgp-compositor                          │
  │   Composites LUNA_ISLAND surface over the desktop     │
  │   Delivers input events to luna-island                │
  └──────────────────────────────────────────────────────┘
```

luna-island is:
- A **separate process** from luna-ai-d (DL-044, Volume IV/00_luna_runtime.md)
- An **LGP client** that holds a `LUNA_ISLAND` surface type
- A **D-Bus subscriber** to `org.lunaos.luna` signals
- The **only process** that may create a `LUNA_ISLAND` surface (enforced by compositor policy)

luna-island is **not**:
- An AI decision-maker (all decisions come from luna-ai-d)
- A notification daemon (luna-notif owns notifications)
- A system status display (luna-bar owns status)
- An application launcher (luna-shell owns the launcher)

---

## Architecture

### Process Architecture

```
Process: luna-island

  Main Thread:
    ├── LGP event loop (compositor socket)
    │     ├── LGP_FRAME_CALLBACK → trigger render
    │     ├── LGP_INPUT_EVENT    → process touch/click
    │     ├── LGP_THEME_CHANGED  → reload theme colors
    │     └── LGP_OUTPUT_CHANGED → reposition if display changes
    │
    └── D-Bus subscriber thread
          ├── org.lunaos.luna.ModeChanged      → state transition
          ├── org.lunaos.luna.ExpressionChanged → expression update
          └── org.lunaos.luna.TokenReceived    → stream LUNA response text

  Render Thread (dedicated):
    └── Receives render commands from main thread
        Writes pixels to LGP shared memory buffer
        Signals main thread when frame is ready
        Uses: Cairo (v1) / Vulkan surface (v2)
```

### Surface Geometry

```
Default position: Bottom-right corner of the primary display
                  Margin: 24px from screen edge

  ┌──────────────────────────────────────────────────────────┐
  │                    Primary Display                        │
  │                                                           │
  │                                                           │
  │                                               ┌────────┐  │
  │                                               │        │  │
  │                                               │ LUNA   │  │
  │                                               │ ISLAND │  │
  │                                               │        │  │
  │                                               └────────┘  │
  │                                               ← 24px →    │
  └──────────────────────────────────────────────────────────┘

AMBIENT state dimensions:     64 × 64 px  (circular surface)
COMPACT_PANEL state:          320 × 120 px (expanded upward)
FULL_CONVERSATION state:      400 × 600 px (expanded upward + left)

Expansion origin:             bottom-right corner (fixed anchor point)
```

### LGP Surface Configuration

```c
// luna-island surface creation request
lgp_create_surface_t create = {
    .surface_type  = LGP_SURFACE_LUNA_ISLAND,   // privileged type
    .layer         = LGP_LAYER_SYSTEM_OVERLAY,  // 500 — above apps, below lock
    .x             = display_width - 64 - 24,   // 24px right margin
    .y             = display_height - 64 - 24,  // 24px bottom margin
    .width         = 64,
    .height        = 64,
    .flags         = LGP_SURFACE_FLAG_TRANSPARENT_BACKGROUND
                   | LGP_SURFACE_FLAG_INPUT_PASSTHROUGH_OUTSIDE
};
```

`LGP_SURFACE_FLAG_INPUT_PASSTHROUGH_OUTSIDE` means: input events that fall outside the Island's visible bounds are passed through to whatever surface is behind. This ensures the Island never accidentally blocks clicks to applications beneath it.

---

## Visual States

Luna Island has three visual states. All transitions between states use the `Expand` or `Collapse` motion class (Volume III / 05_animation_engine.md).

### State 1: AMBIENT

**Trigger:** Default state. Entered when no interaction is in progress and mode is any non-conversation mode.

**Description:** A compact, circular presence indicator. Shows LUNA's current mode through color and subtle animation. Does not display text. Does not occupy significant screen space.

```
AMBIENT state layout (64 × 64 px):

  ╔══════════════════════════╗
  ║                          ║
  ║      ┌──────────┐        ║
  ║      │  ●       │        ║   ← Semantic color dot (mode indicator)
  ║      │   ~~~~~  │        ║   ← Subtle pulse animation (breathing)
  ║      └──────────┘        ║
  ║                          ║
  ╚══════════════════════════╝

Color meaning (semantic colors from theme engine):
  AMBIENT mode    → LUNA_WHITE (neutral)
  DEVSHELL mode   → LUNA_GREEN (operational)
  FOCUS mode      → LUNA_AMBER (concentrated)
  STUDY mode      → LUNA_WHITE (quiet)
  CREATIVE mode   → LUNA_GREEN (energized)
  GAMING mode     → LUNA_VOID  (non-intrusive, minimal)
  CONVERSING      → LUNA_GREEN (active)
  luna-ai-d down  → LUNA_VOID  (greyed out, no pulse)
```

**Animation:** The ambient pulse is a continuous `sin(t)` opacity oscillation from 0.7 to 1.0, period 4 seconds. This is the Living Interface "breathing" — LUNA is always alive, even when idle.

---

### State 2: COMPACT_PANEL

**Trigger:** Short click on the AMBIENT island (DL-034). Dismissed by a second click, an Escape key press, or clicking outside the panel bounds.

**Description:** Luna Island expands upward from the AMBIENT circle into a compact panel. Shows LUNA's current mode label, a brief context summary (what the user is working on), and an affordance to continue to full conversation.

```
COMPACT_PANEL state layout (320 × 120 px):

  ╔══════════════════════════════════════════════════════════╗
  ║  ●  DEVSHELL MODE                              [×]       ║  ← Mode + close
  ╠══════════════════════════════════════════════════════════╣
  ║  Working on:  lunagui/layout.c                           ║  ← Context summary
  ║  Session:     2h 14m                                     ║  ← Time in session
  ╠══════════════════════════════════════════════════════════╣
  ║  Ask LUNA something...                    [Hold to talk] ║  ← Input affordance
  ╚══════════════════════════════════════════════════════════╝

Width:  320px (fixed)
Height: 120px (fixed)
Anchor: bottom-right (expands upward)
```

**Data sources:**
- Mode label and color: `org.lunaos.luna.GetMode()` D-Bus call (cached from last `ModeChanged`)
- Context summary: `org.lunaos.luna.GetContext()` D-Bus call (fetched on expansion)
- Session time: local timer (luna-island tracks time since session start)

**Input affordance:** Tapping the text field transitions to `FULL_CONVERSATION`. Long-pressing the Island from this state also transitions to `FULL_CONVERSATION`.

---

### State 3: FULL_CONVERSATION

**Trigger:** Long press on AMBIENT (≥ 500ms), or tapping the input field in COMPACT_PANEL. Dismissed by a tap on [×], Escape, or clicking/tapping outside the panel bounds.

**Description:** Luna Island expands further into a full conversational interface. LUNA's response text streams in as tokens are received. The user can type or (if enabled) speak. Previous exchanges in the current session are visible.

```
FULL_CONVERSATION state layout (400 × 600 px):

  ╔══════════════════════════════════════════════════════════════╗
  ║  ●  LUNA                                           [×] [↗]  ║  ← Header
  ╠══════════════════════════════════════════════════════════════╣
  ║                                                              ║
  ║  ┌───────────────────────────────────────────────────────┐  ║
  ║  │ LUNA: I noticed you've been in layout.c for a while.  │  ║
  ║  │       The alignment issue is likely in the flex       │  ║
  ║  │       shrink calculation. Want me to trace it?        │  ║  ← LUNA response
  ║  └───────────────────────────────────────────────────────┘  ║
  ║                                                              ║
  ║  ┌───────────────────────────────────────────────────────┐  ║
  ║  │ Me: Yeah trace the shrink logic                       │  ║  ← User message
  ║  └───────────────────────────────────────────────────────┘  ║
  ║                                                              ║
  ║  ┌───────────────────────────────────────────────────────┐  ║
  ║  │ LUNA: ▌                                               │  ║  ← Streaming
  ║  └───────────────────────────────────────────────────────┘  ║
  ║                                                              ║
  ╠══════════════════════════════════════════════════════════════╣
  ║  [  Type or speak...                              🎤  ▶  ]  ║  ← Input bar
  ╚══════════════════════════════════════════════════════════════╝

Width:  400px
Height: 600px
Anchor: bottom-right (expands upward and leftward)
[↗] button: detaches conversation into a larger floating window (v1.5)
```

**Streaming:** Tokens received via `org.lunaos.luna.TokenReceived` D-Bus signal are appended to the current LUNA message bubble character by character. When `is_final = true`, the streaming cursor (▌) is removed and the message is finalized.

**Voice input:** The 🎤 button is visible only when TTS/STT is enabled (DL-041). Disabled by default.

---

## State Transition Machine

```
Luna Island state transitions:

  ┌─────────────────────────────────────────────────────────────┐
  │                      UNINITIALIZED                           │
  │  (luna-island starting, LGP not yet connected)               │
  └────────────────────────┬────────────────────────────────────┘
                           │ LGP connected + compositor ready
                           │ Query luna-ai-d: GetMode()
                           ▼
  ┌─────────────────────────────────────────────────────────────┐
  │                        AMBIENT                               │
  │  Compact 64×64 presence indicator                            │
  │  Breathing pulse animation                                   │
  │  Color = semantic color for current mode                     │
  └──┬───────────────────────────────────────────────────────┬──┘
     │ short click                     luna-ai-d ModeChanged  │
     ▼                                 (stays in AMBIENT, updates color)
  ┌──────────────────┐
  │  COMPACT_PANEL   │
  │  320 × 120 px    │──────────────────────────┐
  │  Mode + context  │  tap input field          │
  └──┬───────────────┘  or long-press            │
     │ [×] / Escape /   (≥500ms from any state)  │
     │ click outside                             ▼
     │                              ┌──────────────────────┐
     └──────────────────────────────│  FULL_CONVERSATION   │
                                    │  400 × 600 px        │
                                    │  Streaming response  │
                                    │  Input bar           │
                                    └──────────┬───────────┘
                                               │ [×] / Escape /
                                               │ click outside
                                               ▼
                                           AMBIENT
```

**Motion choreography for all transitions:**
- `AMBIENT → COMPACT_PANEL`: `LunaMotion.Expand`, 280ms, `ease-out-quint`
- `COMPACT_PANEL → AMBIENT`: `LunaMotion.Collapse`, 220ms, `ease-in-quint`
- `COMPACT_PANEL → FULL_CONVERSATION`: `LunaMotion.Expand`, 320ms, `ease-out-quint`
- `FULL_CONVERSATION → AMBIENT`: `LunaMotion.Collapse`, 260ms, `ease-in-quint`
- `AMBIENT → FULL_CONVERSATION` (long press): `LunaMotion.Expand`, 350ms, `ease-out-expo`

All expansion animations originate from the bottom-right anchor point. The Island grows upward and leftward. It never moves its bottom-right corner.

---

## Expression System

The Expression System translates `ExpressionChanged` D-Bus signals from the Presence Engine into visual changes on the Island surface.

### Expression Signal Format

```c
// D-Bus signal: org.lunaos.luna.ExpressionChanged
// Payload (dict):
{
    "expression_type":  string,   // see Expression Types below
    "intensity":        double,   // 0.0–1.0
    "color_override":   string,   // optional: semantic color name or ""
    "duration_ms":      uint32,   // how long this expression lasts (0 = until next signal)
    "animation_class":  string,   // LunaMotion class to use for transition
}
```

### Expression Types

| Expression Type | Visual Rendering | When Used |
|---|---|---|
| `PULSE_GENTLE` | Slow opacity pulse, 4s period | AMBIENT idle state |
| `PULSE_ALERT` | Fast opacity pulse, 1s period | New notification, needs attention |
| `GLOW` | Radial glow emanating from dot | LUNA is thinking / processing |
| `RIPPLE` | Circular ripple outward from center | New LLM token received / responding |
| `FLASH` | Single brief brightness spike | Error occurred / important alert |
| `DIM` | Reduced opacity (0.4), no pulse | GAMING mode, minimal presence |
| `VOID` | Zero opacity (fade out) | luna-ai-d unavailable |
| `SHIMMER` | Color shift cycle through spectrum | CREATIVE mode active |
| `FOCUS_RING` | Animated ring around the dot | FOCUS mode active |
| `COLLAPSE` | Shrink and fade to nothing | System shutdown animation |

### Expression Priority

The Presence Engine sends expressions at different priority levels. luna-island honors the priority queue:

```
Priority (highest wins):
  7 — SYSTEM_CRITICAL    (OOM, hardware failure — overrides everything)
  6 — LUNA_ALERT         (LUNA has something urgent to say)
  5 — USER_INTERACTION   (user is actively using the Island)
  4 — AI_RESPONSE        (LUNA is responding to a request)
  3 — CONTEXT_CHANGE     (mode change, new application focused)
  2 — AMBIENT_BEHAVIOR   (breathing, idle expressions)
  1 — BACKGROUND         (very subtle passive animations)
```

A higher-priority expression replaces a lower-priority one immediately. When the higher-priority expression ends (its `duration_ms` expires), luna-island returns to the previous expression in the queue.

---

## Multi-Display Behavior

```
Display configuration rules:

  Single display:
    Luna Island → primary display, bottom-right corner

  Multi-display (same setup as multi-display DL — pending):
    Luna Island → primary display (user-designated), bottom-right corner
    Behavior on other displays: none (Island does not clone)
    User may move Island to a different display via luna-settings (v1.5)

  Display hotplug:
    LGP_OUTPUT_CHANGED received → recalculate position for new display geometry
    If primary display removed → Island moves to next available display
```

---

## Interaction Detail

### Click / Tap Detection

The compositor delivers `LGP_INPUT_EVENT` to luna-island only for events within its surface bounds. luna-island implements:

```c
// Short click vs. long press discrimination
on_button_press(event):
    press_start_time = event.timestamp
    start_long_press_timer(500ms)

on_button_release(event):
    elapsed = event.timestamp - press_start_time
    cancel_long_press_timer()
    if elapsed < 500ms:
        handle_short_click()
    // else: long press was already handled by timer callback

on_long_press_timer_expired():
    handle_long_press()
```

### Dismiss Behavior

The compositor delivers click events for surfaces underneath the Island when `INPUT_PASSTHROUGH_OUTSIDE` is set. luna-island tracks whether a click occurred outside its bounds and dismisses the panel accordingly:

```c
on_input_outside_bounds():
    if current_state == COMPACT_PANEL or FULL_CONVERSATION:
        transition_to(AMBIENT)
```

This behavior must not interfere with the user's application interaction — the dismiss happens first, then the click passes through to the application.

---

## Failure Modes

```
Failure scenarios:

  luna-ai-d unavailable (crash or shutdown):
    → luna-island receives D-Bus name lost signal
    → Transition expression to VOID (fade out dot to 0.2 opacity)
    → Set color to LUNA_VOID (gray)
    → Surface remains visible but indicates AI is unavailable
    → COMPACT_PANEL and FULL_CONVERSATION are disabled until luna-ai-d reconnects
    → When luna-ai-d comes back: ModeChanged signal received → restore normal state

  luna-island crash recovery:
    → luna-init restarts luna-island (SIGCHLD handler)
    → luna-island starts fresh: queries GetMode() from luna-ai-d
    → Renders AMBIENT state with current mode color
    → No visible gap for user (restart takes < 1s)

  Compositor unavailable:
    → luna-island's LGP socket read fails → graceful exit
    → luna-init detects exit → does NOT restart immediately (compositor is the bigger problem)
    → When compositor restarts → luna-init restarts luna-island in Stage 6 sequence

  OOM condition (Ollama killed):
    → luna-ai-d sends ExpressionChanged: type=FLASH, color_override=LUNA_AMBER
    → Brief amber flash indicating disruption
    → luna-ai-d sends ExpressionChanged: type=GLOW (recovering)
    → Normal state returns when Ollama restarts
```

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| Render time per frame (AMBIENT) | < 2ms | 4ms |
| Render time per frame (COMPACT_PANEL) | < 4ms | 8ms |
| Render time per frame (FULL_CONVERSATION) | < 8ms | 16ms |
| RAM usage (process) | < 40 MB | 80 MB |
| CPU usage (AMBIENT idle) | < 0.5% | 1% |
| D-Bus round-trip on expansion (GetContext) | < 50ms | 100ms |
| State transition animation frame drop rate | 0 dropped | < 1 per transition |

**Why these targets:** luna-island is an ambient presence layer. It must never become noticeable as a CPU or memory consumer. The Island should feel weightless.

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| luna-island is a separate process from luna-ai-d | DL-044, Volume IV/00 | ✅ Accepted |
| Short click = COMPACT_PANEL, long press = FULL_CONVERSATION | DL-034 | ✅ Accepted |
| Conversation panel owned by luna-island (not a separate process) | DL-044 | ✅ Accepted |
| luna-island communicates with luna-ai-d via D-Bus only | Volume IV/00 | ✅ Accepted |
| LUNA_ISLAND is a privileged LGP surface type | Volume III/01 | ✅ Accepted |
| Island default position: bottom-right, 24px margin | This document | ✅ Accepted |
| Expansion anchor: bottom-right corner (fixed) | This document | ✅ Accepted |
| AMBIENT size: 64×64px | This document | ✅ Accepted |
| COMPACT_PANEL size: 320×120px | This document | ✅ Accepted |
| FULL_CONVERSATION size: 400×600px | This document | ✅ Accepted |
| Cairo for v1 rendering, Vulkan path for v2 | DL-026 / This document | 🧪 Experimental |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Multi-display Island position.** Which display does Luna Island appear on in a multi-display setup? Can the user move it? Target: v1.5. Must be a Decision Log entry.

2. **Detach to window (`[↗]` button).** FULL_CONVERSATION has a detach affordance for v1.5. What surface type does the detached conversation window use? A standard `APPLICATION_WINDOW` surface? A new `LUNA_CONVERSATION` surface type? Must be a Decision Log entry before v1.5 planning.

3. **Island shape in AMBIENT.** This document describes a circular dot. Should the Island be a true circle (using compositor mask), a rounded rectangle, or a simple square with rounded corners? The compositor must support non-rectangular input regions for accurate hit-testing. Must be resolved before Stage 3 implementation.

4. **Context summary source.** `GetContext()` returns active app + file type. Who populates the "session time" displayed in COMPACT_PANEL — luna-island (measures its own uptime) or luna-ai-d (measures session duration)? Must be a Decision Log entry.

5. **Notification badge.** Should luna-island show a badge count for unread notifications (received from luna-notif via D-Bus)? This is a minor UI decision but affects the AMBIENT surface layout.

---

## AI Context

- luna-island **renders** LUNA's presence. It does not **decide** LUNA's behavior. If you are writing code that makes a decision (what expression to show, what mode LUNA is in), it belongs in `luna-ai-d`, not `luna-island`.
- All state transitions in luna-island are triggered by external events (D-Bus signals, LGP input events). luna-island never initiates a state change on its own.
- The three states are AMBIENT, COMPACT_PANEL, and FULL_CONVERSATION. No other states exist in v1.
- The expansion anchor is always the bottom-right corner. Never change this in code — all expansion math must be relative to this anchor.
- When in doubt about what to render, luna-island falls back to AMBIENT state. Never render nothing (except `VOID` expression, which renders a barely-visible dot, not an invisible surface).
- luna-island must handle D-Bus signals arriving mid-animation gracefully — do not interrupt a motion-class animation to immediately render a new state. Queue the state change and apply it after the animation completes.

---

*Document: `Volume III / 07_luna_island.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 03_compositor.md, 05_animation_engine.md, 06_theme_engine.md, Volume IV/00_luna_runtime.md, DL-034, DL-044*
*Informs: Volume IV/01_presence_engine.md, Volume V/01_shell.md*
