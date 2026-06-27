# Mahina — Window Objects
**Volume III · Chapter 8**
**Classification:** Core Architecture — Graphics & Presence
**Status:** Canonical · Defines all LGP surface types, their properties, and lifecycle rules

---

## Purpose

This document defines **every window object type** that exists in Mahina. A window object is anything the compositor manages as a drawable surface. It is not limited to what users think of as "windows" — it includes panels, overlays, menus, the lock screen, Luna Island, and every other visible element.

Understanding window objects is required for:
- Writing a compositor (what surface types to support)
- Writing LunaGUI (what surface type does a widget tree create)
- Writing luna-shell (what surface type does the desktop use)
- Writing any application (what surface type should my app request)

---

## Overview

Every visible element in Mahina is a **surface** — a rectangular region of pixel data managed by the compositor. Surfaces are created by LGP clients and composited together by lgp-compositor according to their layer ordering, geometry, and opacity.

```
Compositor surface stack (top = rendered last = visually on top):

  Layer 700 — CURSOR           (mouse pointer, hardware cursor)
  Layer 600 — SYSTEM_MODAL     (lock screen, authentication dialogs)
  Layer 500 — SYSTEM_OVERLAY   (Luna Island, notification toasts)
  Layer 400 — TOP_LAYER        (always-on-top windows, if supported)
  Layer 300 — APPLICATION      (normal application windows)
  Layer 200 — SHELL_PANEL      (status bar, dock, app launcher panel)
  Layer 100 — WALLPAPER        (desktop background)
  Layer   0 — COMPOSITOR_ROOT  (compositor framebuffer — never a client surface)
```

Within the same layer, surfaces are ordered by creation time (newest on top) unless the compositor receives an explicit z-order change request.

---

## Surface Type Reference

### WALLPAPER Surface

| Property | Value |
|---|---|
| Layer | 100 |
| Created by | luna-shell |
| Max instances | 1 per display output |
| Transparency | Opaque (no alpha) |
| Input | None (input passes through) |
| Resize | Yes — matches display resolution |

**Description:** The desktop background. luna-shell creates one per connected display. It renders the wallpaper image or a procedural wallpaper generator's output. The wallpaper surface never receives input — all clicks and gestures pass through to application surfaces above it.

**Notes:** If luna-shell crashes, the compositor renders its fallback background color (from `LunaTheme.void_background`) until luna-shell restarts.

---

### SHELL_PANEL Surface

| Property | Value |
|---|---|
| Layer | 200 |
| Created by | luna-shell (top panel), luna-bar, luna-dock |
| Max instances | Unlimited (multiple panels allowed) |
| Transparency | Partial alpha (glass effect) |
| Input | Yes — full input routing |
| Resize | Fixed height, full display width |

**Description:** System UI panels: the status bar (luna-bar) at the top, the dock at the bottom. Shell panels are permanent fixtures of the desktop. They have a fixed height and span the full width of their display.

**Geometry rules:**
- Top panel (luna-bar): `x=0, y=0, width=display_width, height=32px`
- Bottom dock (luna-dock): `x=0, y=display_height-64, width=display_width, height=64px`

**Exclusion zones:** Shell panels register **exclusion zones** with the compositor. Application windows are not maximized into exclusion zones — they stop at the panel boundary. The compositor tracks these zones and sends them to application clients that request `LGP_GET_WORKSPACE_GEOMETRY`.

```
Exclusion zone diagram:

  ┌──────────────────────────────────────────────────────┐
  │  luna-bar (height: 32px) — Exclusion zone top        │  ← SHELL_PANEL
  ├──────────────────────────────────────────────────────┤
  │                                                       │
  │              APPLICATION WINDOWS                      │
  │           (bounded by exclusion zones)                │
  │                                                       │
  ├──────────────────────────────────────────────────────┤
  │  luna-dock (height: 64px) — Exclusion zone bottom    │  ← SHELL_PANEL
  └──────────────────────────────────────────────────────┘
```

---

### APPLICATION_WINDOW Surface

| Property | Value |
|---|---|
| Layer | 300 |
| Created by | Any LunaGUI application |
| Max instances | Unlimited |
| Transparency | Partial alpha (window chrome may be transparent) |
| Input | Yes — full input routing while focused |
| Resize | Yes — via LGP resize events |

**Description:** The standard application window. This is what the user thinks of as a "window" — a titled, resizable, focusable surface that holds application content.

**Subsurfaces:** An APPLICATION_WINDOW may have **subsurfaces** — child surfaces that are positioned relative to the parent window. Subsurfaces are used for:
- Dropdown menus
- Tooltip overlays
- Video frames (hardware-decoded video presented as a subsurface)
- Popup dialogs

```
APPLICATION_WINDOW anatomy:

  ┌──────────────────────────────────────────────────────┐
  │  Window Chrome (Luna Dark glass effect)               │
  │  ┌─────────────────────────────────────────────────┐ │
  │  │  Title bar (32px) — rendered by LunaGUI         │ │
  │  │  [App Icon]  Window Title         [–][□][×]     │ │
  │  └─────────────────────────────────────────────────┘ │
  │  ┌─────────────────────────────────────────────────┐ │
  │  │                                                   │ │
  │  │  Client area (application renders here)          │ │
  │  │                                                   │ │
  │  └─────────────────────────────────────────────────┘ │
  └──────────────────────────────────────────────────────┘

  Client rendering boundary = inside the chrome border
  Chrome is rendered by LunaGUI using the active theme's
  window_chrome tokens
```

**States:**
- `NORMAL` — standard windowed size at a user-defined position
- `MAXIMIZED` — fills the workspace area (respects exclusion zones)
- `MINIMIZED` — removed from compositor stack (surface still exists but is not rendered)
- `FULLSCREEN` — covers the entire display, overrides exclusion zones (games, video)

---

### TOP_LAYER Surface

| Property | Value |
|---|---|
| Layer | 400 |
| Created by | Privileged applications (declared in AppArmor profile) |
| Max instances | Limited by compositor policy |
| Transparency | Yes |
| Input | Yes |
| Resize | Yes |

**Description:** Always-on-top surfaces that stay above all APPLICATION_WINDOW surfaces regardless of focus. Intended for: picture-in-picture video, floating notes, developer HUD overlays.

**Policy:** An application must declare `capabilities = ["top_layer"]` in its `luna.toml` manifest and have an AppArmor profile that permits the `LGP_CAP_LAYER_SHELL` capability (DL-033). The compositor validates this at surface creation.

---

### LUNA_ISLAND Surface

| Property | Value |
|---|---|
| Layer | 500 |
| Created by | luna-island only (enforced by compositor) |
| Max instances | 1 per display |
| Transparency | Yes — full alpha |
| Input | Yes — within bounds only, passthrough outside |
| Resize | Yes — state-driven (AMBIENT/COMPACT_PANEL/FULL_CONVERSATION) |

**Description:** The Luna Island presence surface. Fully specified in `Volume III / 07_luna_island.md`. The compositor enforces that only the process registered as the Island owner (validated via `SO_PEERCRED`) may create this surface type.

---

### NOTIFICATION_TOAST Surface

| Property | Value |
|---|---|
| Layer | 500 |
| Created by | luna-notif |
| Max instances | Up to 5 simultaneous toasts |
| Transparency | Yes — glass effect |
| Input | Yes — click to dismiss / activate |
| Resize | Fixed width (380px), variable height (content-driven) |

**Description:** Notification toast popups created by luna-notif when a new notification arrives. They appear in the top-right corner (or top-center on narrow displays) and auto-dismiss after their timeout.

```
Toast positioning (stacking):

  ┌──────────────────────────┐  ← Toast 1 (newest)  y = panel_height + 8
  └──────────────────────────┘
  ┌──────────────────────────┐  ← Toast 2           y = toast1.bottom + 8
  └──────────────────────────┘
  ┌──────────────────────────┐  ← Toast 3           y = toast2.bottom + 8
  └──────────────────────────┘

Max stack: 5 toasts. If > 5 arrive: oldest is dismissed first.
```

---

### CANVAS_SURFACE Surface

| Property | Value |
|---|---|
| Layer | 300 (but flags may elevate to fullscreen) |
| Created by | Applications declaring `capabilities = ["canvas"]` |
| Max instances | 1 per application |
| Transparency | No — opaque RGBA buffer |
| Input | Yes — raw, unprocessed |
| Resize | Yes |

**Description:** A direct-rendering surface for applications that need full pixel control: games, video players, graphics editors. CANVAS_SURFACE bypasses the LunaGUI widget system entirely. The application writes directly to an RGBA pixel buffer (or provides a GPU-rendered DMA-BUF buffer).

```
CANVAS_SURFACE data path:

  Application GPU → DMA-BUF buffer → LGP_COMMIT_BUFFER
                                          ↓
                                    compositor imports DMA-BUF
                                    (zero-copy — DL-026)
                                          ↓
                                    composited onto display
```

**Color:** CANVAS_SURFACE does not use the semantic color system (DL-025). Applications write raw RGBA values. The compositor does not apply theme colors to CANVAS_SURFACEs.

---

### SYSTEM_MODAL Surface

| Property | Value |
|---|---|
| Layer | 600 |
| Created by | luna-lock, luna-init authorized processes |
| Max instances | 1 (compositor enforces; second creation attempt is rejected) |
| Transparency | No — fully opaque |
| Input | Yes — captures all input, none passes through |
| Resize | Fixed: full display coverage |

**Description:** A full-display surface that captures all input and occludes everything beneath it. Used by: luna-lock (DL-035), emergency system dialogs (disk full, hardware failure).

**Security:** Only processes with `SO_PEERCRED` credentials matching the compositor's SYSTEM_MODAL policy list may create this surface type. luna-lock and an emergency process started by luna-init are the only permitted creators in v1.

**Behavior:** When a SYSTEM_MODAL surface exists, all other surfaces receive no input events. The compositor still composites them (they are visible beneath the modal) but they cannot be interacted with.

---

### CURSOR Surface

| Property | Value |
|---|---|
| Layer | 700 |
| Created by | lgp-compositor (hardware cursor plane) |
| Max instances | 1 per display |
| Transparency | Yes |
| Input | N/A |
| Resize | Fixed: cursor image size |

**Description:** The mouse cursor. In v1, the compositor uses the system's default cursor set. Applications that request custom cursors (via `lgp_ext_cursor_shape_v1`) send cursor image data to the compositor, which updates the hardware cursor plane. The cursor surface is always managed by the compositor — no LGP client creates it directly.

---

## Surface Lifecycle

```
Surface lifecycle state machine (all types):

  [LGP client connects]
         │
         │ LGP_CREATE_SURFACE (type, layer, geometry, flags)
         ▼
   SURFACE_PENDING
   (compositor validates: type allowed? policy ok? geometry valid?)
         │
         ├──→ REJECTED (policy violation, type not allowed)
         │         └──→ [client receives LGP_ERROR, may disconnect]
         │
         │ compositor accepts
         ▼
   SURFACE_CREATED (not yet visible — no buffer committed)
         │
         │ client: shm_open() → render → LGP_COMMIT_BUFFER
         ▼
   SURFACE_MAPPED (visible, compositor composites it)
         │
         ├──→ SURFACE_OCCLUDED (another surface on top — still mapped, not rendered)
         │         └──→ SURFACE_MAPPED (top surface removed)
         │
         ├──→ SURFACE_MINIMIZED (shell minimize request)
         │         └──→ SURFACE_MAPPED (shell restore)
         │
         └──→ LGP_DESTROY_SURFACE
                   │
                   ▼
             SURFACE_DESTROYED
                   │
             [client may create new surface or disconnect]
```

---

## Subsurfaces

Any `APPLICATION_WINDOW` may create subsurfaces. A subsurface is a child surface that:
- Shares its parent's layer position
- Is positioned relative to the parent's top-left corner
- Is clipped to the parent's bounds (v1: hard clip; v1.5: optional overflow)
- Is destroyed when the parent is destroyed

```c
// Subsurface creation
lgp_create_subsurface_t create = {
    .parent_surface_id = main_window_id,
    .relative_x        = 0,
    .relative_y        = title_bar_height,  // client area starts below title bar
    .width             = client_width,
    .height            = client_height,
    .flags             = 0,
};
```

**Use cases in v1:**
- Dropdown menus (subsurface positioned at menu trigger location)
- Video frames (hardware-decoded DMA-BUF buffer as subsurface)
- Popup tooltips (subsurface near the hovered widget)

---

## Z-Order Rules

Within the same layer, surfaces follow these ordering rules:

1. **Creation order:** Newer surfaces start on top of older surfaces in the same layer
2. **Focus follows:** When a surface receives keyboard focus, it is raised to the top of its layer
3. **Explicit z-order:** The compositor may support `LGP_SET_Z_ORDER` in v1.5 for explicit reordering
4. **Shell management:** luna-shell may request z-order changes for application windows (alt-tab raises a window)

```
Z-order within APPLICATION layer (layer 300):

  [App C] ← top (most recently focused)
  [App B]
  [App A] ← bottom (created first, not focused recently)

  After user alt-tabs to App A:

  [App A] ← top (raised on focus)
  [App C]
  [App B] ← bottom
```

---

## Input Routing

The compositor routes input events to surfaces using these rules in order:

```
Input routing priority:

  1. SYSTEM_MODAL (layer 600) — if exists, receives ALL input regardless of cursor position
  2. CURSOR events — compositor handles cursor movement internally (hardware cursor)
  3. Hit test from top to bottom:
       Layer 700 → 600 → 500 → 400 → 300 → 200 → 100
       First surface whose bounds contain the cursor position receives the event
  4. Passthrough flag: if surface has INPUT_PASSTHROUGH_OUTSIDE, events outside its
     visible bounds pass to the next surface in the hit test
  5. No surface hit: event is consumed (not delivered)
```

**Keyboard focus:** The compositor maintains a single keyboard-focused surface. Keyboard events go only to the focused surface. Mouse/touch events go to the hit-tested surface regardless of keyboard focus.

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Subsurface overflow.** v1 clips subsurfaces to parent bounds. v1.5 may allow overflow (menus that extend beyond the window edge). This requires compositor support for non-rectangular clip regions.

2. **Z-order API.** No `LGP_SET_Z_ORDER` in v1. Shell must use focus-raising only. Is this sufficient for alt-tab and window management? Must be evaluated before Stage 3.

3. **CANVAS_SURFACE fullscreen flag.** When a CANVAS_SURFACE requests fullscreen, it should cover the entire display including the status bar. Does it go to layer 599 (below SYSTEM_MODAL but above SYSTEM_OVERLAY) or does it get a dedicated FULLSCREEN layer? Must be a Decision Log entry.

4. **Per-display cursor surfaces.** Multi-display setups require one cursor per display. Does the compositor manage a hardware cursor plane per display via KMS, or is there a single cursor that migrates? Must be resolved before multi-display support implementation.

5. **Window decorations ownership.** This document assumes LunaGUI renders the title bar. Should the compositor render window decorations (server-side decorations) instead? Server-side decorations ensure visual consistency across all apps including non-LunaGUI apps. Must be a Decision Log entry.

---

## AI Context

- When writing code that creates a surface, always verify the surface type is correct for the use case. An application NEVER creates a LUNA_ISLAND, SYSTEM_MODAL, or NOTIFICATION_TOAST surface — those are privileged types.
- The layer numbers are fixed constants. Do not use raw numbers in code — use the defined constants (`LGP_LAYER_APPLICATION`, `LGP_LAYER_SYSTEM_OVERLAY`, etc.).
- Exclusion zones are published by shell panels. Any application that requests `LGP_GET_WORKSPACE_GEOMETRY` should honor the exclusion zones when deciding its initial position and maximum size.
- The hit-test order defines who gets input. If a surface is not receiving input events you expect, check whether a higher-layer surface is intercepting them. LUNA Island with `INPUT_PASSTHROUGH_OUTSIDE` is the most common source of confusion.
- CANVAS_SURFACE skips the semantic color system and LunaGUI entirely. Do not use CANVAS_SURFACE for a standard application — only use it when you need direct pixel control (game, video, custom renderer).

---

*Document: `Volume III / 08_window_objects.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 03_compositor.md, 07_luna_island.md, DL-034, DL-035*
*Informs: Volume V/01_shell.md, Volume III/09_visual_language.md*
