# Mahina — LunaGUI Toolkit
**Volume III · Chapter 4**
**Classification:** Core Architecture — Application Interface
**Status:** Active · Primary development interface for all Mahina applications

---

## Purpose

This document specifies the **LunaGUI toolkit**: the standard application interface for Mahina. LunaGUI is the layer that application developers use to build graphical applications. It sits above the Luna Graphics Protocol (LGP) and provides widgets, layout, rendering, and animation APIs that automatically comply with Living Interface rules without requiring developers to know LGP's wire format.

This document is the authoritative reference for:
- LunaGUI's architectural position in the graphics stack
- Widget primitives and the widget tree model
- The layout engine model
- The rendering API (canvas drawing)
- The animation API and its relationship to the LGP Motion Vocabulary
- The event handling model
- LunaGUI's language binding strategy
- What LunaGUI does and does not provide

---

## Overview

LunaGUI is a **hybrid toolkit** (DL-004R): it provides both a high-level widget layer and a low-level rendering API, so applications can use whichever level of abstraction is appropriate for their needs.

A settings panel uses LunaGUI's widget primitives (buttons, text inputs, toggles, lists). A code editor uses the canvas rendering API for its text rendering. A media player uses a LunaGUI window shell with a direct-LGP CANVAS_SURFACE for the video viewport. All three are LunaGUI applications — they just use different parts of the toolkit.

LunaGUI is not optional for normal applications. It is the standard. Direct-LGP access without LunaGUI is reserved for specialized components (games, video decoders, custom GPU renderers) that have a genuine need to bypass the widget layer.

---

## Design Philosophy

### LunaGUI propagates Living Interface rules to application developers

The single most important design constraint: LunaGUI must make it easy to follow Living Interface rules and difficult to violate them.

Concretely:
- LunaGUI's color API does not accept hex values for system-semantic elements. It accepts semantic names (`LunaColor.Healthy`, `LunaColor.Critical`, `LunaColor.Warning`). The toolkit maps these to the Color Semantic Contract codes it sends via LGP.
- LunaGUI's animation API does not accept arbitrary easing functions and durations. It accepts motion class names from the Motion Vocabulary (`LunaMotion.Ripple`, `LunaMotion.ScanlingSweep`) and respects the Animation Budget automatically.
- A developer who uses LunaGUI's standard APIs cannot accidentally violate the Color Semantic Contract or the Motion Vocabulary without bypassing the toolkit's API layer.

The toolkit is the compliance layer. Application developers get safety for free.

### Widgets are opinionated

LunaGUI widgets have a defined visual appearance that follows the active Mahina theme. Applications do not fully style their own widgets. They can:
- Set semantic color state (operational, warning, critical) on widgets
- Apply supported size and spacing variants
- Use the canvas rendering API for custom content within a widget's client area

Applications cannot:
- Arbitrarily restyle a button to look like something outside the Mahina design system
- Use colors that have no semantic equivalent for system-semantic surfaces
- Add motion types not in the Motion Vocabulary to widget transitions

This is intentional. Visual consistency across Mahina applications is a system property, not an application property.

### LunaGUI is not Electron

LunaGUI does not use a web rendering engine, a JavaScript runtime, or HTML/CSS for layout. It is a native toolkit. The performance characteristics are those of native C/Rust code, not of a browser embedded in an application.

---

## Architecture

### Position in the Graphics Stack

```
┌─────────────────────────────────────────────────────┐
│              APPLICATION CODE                         │
│                                                       │
│  LunaGUI API:                                         │
│  window = LunaWindow.new()                            │
│  button = LunaButton("Save", on_click=save_handler)   │
│  window.add(button)                                   │
│  window.show()                                        │
└───────────────────────┬─────────────────────────────┘
                        │  LunaGUI C API calls
┌───────────────────────▼─────────────────────────────┐
│                 LunaGUI Library                       │
│                                                       │
│  Widget tree  │  Layout engine  │  Style resolver    │
│  Event system │  Canvas API     │  Animation API     │
│  Accessibility│  Text engine    │  Image decoder     │
└───────────────────────┬─────────────────────────────┘
                        │  LGP protocol messages
┌───────────────────────▼─────────────────────────────┐
│              LGP Compositor                           │
└─────────────────────────────────────────────────────┘
```

### Widget Tree Model

LunaGUI is a **retained-mode** GUI toolkit. Applications build a tree of widget objects and LunaGUI manages rendering that tree efficiently, only re-rendering widgets that have changed state.

```
LunaWindow (root)
    └── LunaContainer (vertical layout)
            ├── LunaLabel ("Settings")
            ├── LunaContainer (horizontal layout)
            │       ├── LunaLabel ("Wi-Fi")
            │       └── LunaToggle (state: on)
            ├── LunaContainer (horizontal layout)
            │       ├── LunaLabel ("Notifications")
            │       └── LunaToggle (state: on)
            └── LunaButton ("Save")
```

Widgets are objects with:
- **State** — internal state (text content, toggle on/off, progress value)
- **Style** — read from the active theme; can be partially overridden via semantic variants
- **Children** — ordered list of child widgets (for container widgets)
- **Layout hints** — minimum/maximum size, alignment, grow/shrink preferences
- **Event handlers** — callbacks for user interaction (click, focus, text change)

### Layout Engine

```
TODO:
Decision not yet finalized.
Reason: The layout model has not been formally decided.
Candidates:

A: Constraint-based layout (like Apple's Auto Layout / CSS Grid)
   - Powerful, handles complex responsive designs
   - Complex to implement correctly
   - Familiar to developers from web/iOS background

B: Box model layout (like CSS Flexbox / Android LinearLayout)
   - Simpler: a container arranges children in a row or column
   - Nestable box containers achieve most layouts
   - Simpler to implement; less expressive for edge cases

C: Custom Mahina layout model
   - Designed specifically for Mahina's design vocabulary
   - Requires more design work before implementation

Recommendation: Start with Box model layout (B) for v1 — it handles the
vast majority of Mahina application layouts and is fast to implement.
Constraint-based layout can be added in v1.5.
Must be a Decision Log entry before toolkit implementation begins.
```

### Widget Primitives

The following widget types are the v1 LunaGUI primitive set:

**Container widgets:**

| Widget | Description |
|---|---|
| `LunaWindow` | Top-level window. Creates an `APPLICATION_WINDOW` LGP surface. |
| `LunaPanel` | General-purpose container. Supports horizontal/vertical/grid layout. |
| `LunaScrollView` | Scrollable container. Clips content and provides scroll bars. |
| `LunaDialog` | Modal dialog overlay. Creates an `OVERLAY_SURFACE` surface. |
| `LunaPopover` | Transient popup attached to an anchor widget. |

**Content widgets:**

| Widget | Description |
|---|---|
| `LunaLabel` | Displays text. Supports semantic color, weight, size variants. |
| `LunaImage` | Displays a decoded image. Handles scaling and aspect ratio. |
| `LunaCanvas` | Custom rendering surface. Application draws via the Canvas API. |
| `LunaSeparator` | Visual divider line (horizontal or vertical). |
| `LunaProgressBar` | Progress indicator. Maps to `LunaMotion.Pulse` when indeterminate. |
| `LunaSpinner` | Loading indicator. Always maps to `LunaMotion.Pulse`. |

**Interactive widgets:**

| Widget | Description |
|---|---|
| `LunaButton` | Tappable button. Primary, secondary, and destructive variants. |
| `LunaIconButton` | Button displaying only an icon. |
| `LunaToggle` | On/off toggle switch. |
| `LunaCheckbox` | Multi-select checkbox. |
| `LunaRadioButton` | Single-select within a group. |
| `LunaTextInput` | Single-line text entry. |
| `LunaTextArea` | Multi-line text entry. |
| `LunaDropdown` | Selection from a list of options. |
| `LunaSlider` | Range value selector. |
| `LunaSearchInput` | Text input with built-in search icon and clear button. |

**Navigation widgets:**

| Widget | Description |
|---|---|
| `LunaTabBar` | Horizontal tab navigation. |
| `LunaSidebar` | Vertical navigation list. |
| `LunaToolbar` | Horizontal strip of action buttons. |
| `LunaBreadcrumb` | Path navigation indicator. |

```
TODO:
Widget set above is a proposed v1 primitive set. Final list requires
design review against the Mahina design system before implementation.
Add to Volume III / 04_lunagui.md when finalized.
```

---

## Technical Details

### Canvas Rendering API

`LunaCanvas` is the widget that provides the low-level drawing surface. Its rendering API exposes:

**Shapes:**
```
canvas.fill_rect(x, y, width, height, color)
canvas.stroke_rect(x, y, width, height, color, line_width)
canvas.fill_rounded_rect(x, y, width, height, radius, color)
canvas.fill_circle(cx, cy, radius, color)
canvas.fill_path(path, color)
canvas.stroke_path(path, color, line_width)
```

**Text:**
```
canvas.fill_text(text, x, y, font, size, color)
canvas.measure_text(text, font, size) → (width, height)
```

**Images:**
```
canvas.draw_image(image, src_rect, dst_rect, alpha)
```

**Transforms:**
```
canvas.push_transform(matrix)
canvas.pop_transform()
canvas.push_clip(rect)
canvas.pop_clip()
```

**Color in canvas API:**
`LunaCanvas` accepts raw RGBA colors (as `u32` values). This is the intended escape hatch — within a `LunaCanvas` widget, application code has full color freedom. The canvas does not enforce semantic color constraints because canvas content is developer-controlled, not system-semantic.

The canvas surface rendered by `LunaCanvas` is composited as part of the `APPLICATION_WINDOW` surface — it is not a separate `CANVAS_SURFACE` at the LGP protocol level. A separate LGP `CANVAS_SURFACE` is only used for direct-LGP applications that bypass LunaGUI entirely.

### Animation API

LunaGUI's animation API wraps LGP motion class declarations. Application developers use named animations that map to the locked Motion Vocabulary:

```
// LunaGUI animation API
widget.animate(
    animation: LunaAnimation.FadeIn,    // Maps to LunaMotion.Fade
    duration: nil,                       // nil = use Motion Vocabulary default ceiling
    on_complete: callback
)

widget.animate(
    animation: LunaAnimation.SlideIn(direction: .left),  // Maps to LunaMotion.Slide
    duration: 150,   // Explicit duration — must be ≤ 200ms for UI transitions
    on_complete: nil
)
```

**LunaAnimation to LGP motion class mapping:**

| LunaAnimation | LGP Motion Class | Animation Budget ceiling |
|---|---|---|
| `FadeIn` / `FadeOut` | `LunaMotion.Fade` | ≤ 200ms (UI element transition) |
| `SlideIn` / `SlideOut` | `LunaMotion.Slide` | ≤ 200ms |
| `ScaleIn` / `ScaleOut` | `LunaMotion.Scale` | ≤ 200ms |
| `Pulse` | `LunaMotion.Pulse` | ≤ 400ms (state change) |
| `Ripple` (button press response) | `LunaMotion.Ripple` | ≤ 100ms (click response) |
| `WindowOpen` | `LunaMotion.Expand` | ≤ 300ms (window open/close) |
| `WindowClose` | `LunaMotion.Collapse` | ≤ 300ms |
| `ParticleBurst` | `LunaMotion.ParticleBurst` | ≤ 400ms (state change) |
| `ScanlineSweep` | `LunaMotion.ScanlineSweep` | ≤ 2000ms (complex narrative) |

If an application passes a `duration` value that exceeds the Motion Vocabulary ceiling for that animation type, LunaGUI clamps it to the ceiling and logs a debug warning. The Application Budget violation is prevented at the toolkit level, before the LGP message is even sent.

### Event Handling

LunaGUI uses a callback-based event model. Each widget exposes event registration methods:

```
button.on_click(callback: fn())
text_input.on_change(callback: fn(new_text: str))
text_input.on_submit(callback: fn(text: str))
toggle.on_change(callback: fn(new_state: bool))
window.on_close_request(callback: fn() → bool)  // Return false to cancel close
window.on_resize(callback: fn(width: int, height: int))
window.on_focus_change(callback: fn(has_focus: bool))
```

Events are delivered on the main thread. LunaGUI is **single-threaded** from the application's perspective — all event callbacks run sequentially on the main thread. If an application needs background work, it spawns its own threads and posts results back to the main thread via a channel (LunaGUI provides a `LunaMainQueue.post(fn)` mechanism for this).

### Text Rendering

```
TODO:
Decision not yet finalized.
Reason: The text rendering library for LunaGUI has not been decided.
Options:
  A: FreeType + HarfBuzz — industry standard for font rendering + shaping.
     Well-understood upstream dependency (Law I permits this).
     Required for correct internationalization and complex scripts.
  B: A custom text renderer.
     Significant implementation cost. Not recommended for v1.
Recommendation: FreeType + HarfBuzz. Standard, correct, auditable.
Must be a Decision Log entry.
```

LunaGUI ships with a default system font. The default font is:

```
TODO:
Decision not yet finalized.
Reason: The Mahina default typeface has not been selected.
Requirements: Open-source license, Latin + extended Latin + CJK support,
screen-optimized hinting, no telemetry (local font files only).
Candidates: Inter, Noto Sans, IBM Plex Sans.
Must be a Decision Log entry (or a DL-P entry as a pending design decision).
This is related to but separate from DL-P02 (sound design) — both are
visual/audio identity decisions.
```

### Accessibility

```
TODO:
Decision not yet finalized.
Reason: Mahina accessibility architecture has not been specified.
Requirements:
  - Screen reader support (accessibility tree exposed from widget tree)
  - Keyboard navigation for all interactive widgets
  - Reduced-motion mode (see living_interface_design.md Open Question 2)
  - High-contrast mode
LunaGUI's widget tree is the natural source for accessibility semantics.
Minimum v1 requirement: full keyboard navigation.
Screen reader and reduced-motion support: v1.5 target.
```

### Language Bindings

LunaGUI's primary implementation is in **C** (the primary Mahina implementation language). Higher-level bindings will be provided for:

| Language | Status | Notes |
|---|---|---|
| C | Primary | The canonical implementation |
| Rust | Planned (v1) | Safe wrappers around C FFI. Rust is planned as the v2 implementation language (DL-007 analogy for the toolkit) |
| Python | Planned (v1.5) | For scripting and rapid prototyping |

```
TODO:
Decision not yet finalized.
Reason: The binding generation strategy has not been decided.
Options:
  A: Hand-written bindings (full control, maintenance burden).
  B: Bindgen (for Rust) + Cython (for Python) — automated from C headers.
Option B is recommended for v1. Must be a Decision Log entry when binding
work begins.
```

---

## LunaGUI vs. Direct-LGP — Decision Guide

Application developers should choose based on this guide:

| Use case | Use LunaGUI | Use Direct LGP |
|---|---|---|
| Settings application | ✅ | |
| File manager | ✅ | |
| Text editor | ✅ (with LunaCanvas for text area) | |
| Web browser | ✅ (chrome) + possibly LunaCanvas (viewport) | |
| Video player (chrome) | ✅ | |
| Video player (video frame) | | ✅ CANVAS_SURFACE |
| Game (UI overlays) | ✅ | |
| Game (main viewport) | | ✅ CANVAS_SURFACE |
| GPU compute visualizer | | ✅ CANVAS_SURFACE |
| Terminal emulator | ✅ (with LunaCanvas for terminal area) | |
| Mahina shell component | LunaGUI or direct LGP (privileged) | |

The rule: **if it's application chrome (menus, windows, controls), use LunaGUI. If it's a continuous GPU render surface (game viewport, video frame, custom renderer), use direct-LGP CANVAS_SURFACE.**

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| LunaGUI is the standard application interface | DL-004R | Accepted |
| LunaGUI is a hybrid: widgets + canvas API | DL-004R + this document | Accepted |
| LunaGUI enforces Motion Vocabulary at the API level | This document | Accepted |
| LunaGUI uses semantic color API (not hex) for system surfaces | This document | Accepted |
| LunaGUI is retained-mode | This document | Accepted |
| LunaGUI primary implementation language: C | This document | Accepted |
| Rust binding: planned v1 | This document | Provisional |
| Python binding: planned v1.5 | This document | Provisional |
| Layout model: Box model for v1 (requires DL entry) | This document | Provisional |
| Text rendering: FreeType + HarfBuzz (requires DL entry) | This document | Provisional |
| Accessibility: keyboard nav v1, screen reader v1.5 | This document | Provisional |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Layout engine model.** Box model (v1) vs. constraint-based. Must be a Decision Log entry.

2. **Default typeface.** Inter / Noto Sans / IBM Plex Sans. Must be a Decision Log entry.

3. **Text rendering library.** FreeType + HarfBuzz is recommended. Must be a Decision Log entry.

4. **Language binding generation strategy.** bindgen + Cython vs. hand-written. Must be a Decision Log entry when binding work begins.

5. **Widget set finalization.** The v1 primitive list above is a proposal. Requires design review against the Mahina design system.

6. **Accessibility architecture.** Screen reader, reduced-motion mode, high-contrast. v1 minimum: keyboard navigation. Must be scoped before v1.5 planning.

7. **Theming API.** LunaGUI widgets read their appearance from the active theme. The theme data format and the theme selection API have not been specified (deferred to `06_theme_engine.md`).

8. **Living Interface enforcement scope.** `living_interface_design.md` Open Question 1: does LunaGUI enforce Motion Vocabulary and Color Semantic Contract on third-party app content (inside `LunaCanvas` areas)? Current answer: no — `LunaCanvas` is the explicit escape hatch. Needs formal resolution.

---

## AI Context

An AI agent building a LunaGUI application must understand:

- Use `LunaColor.Healthy`, `LunaColor.Critical`, `LunaColor.Warning`, `LunaColor.Neutral`, `LunaColor.Void` for system-semantic colors. Do not pass arbitrary hex values to system-semantic widget properties.
- Use `LunaAnimation.FadeIn`, `LunaAnimation.SlideIn`, etc. for transitions. Do not specify custom easing curves. The toolkit enforces budget ceilings — if you set `duration: 500` on a `FadeIn`, it will be clamped to 200ms.
- All event callbacks run on the main thread. Do not perform blocking operations inside event callbacks. Spawn background threads and use `LunaMainQueue.post()` to deliver results.
- `LunaCanvas` inside a LunaGUI application is not the same as a direct-LGP `CANVAS_SURFACE`. It is composited as part of the `APPLICATION_WINDOW` surface. Direct-LGP `CANVAS_SURFACE` is only for applications that don't use LunaGUI at all (or for a specific standalone render surface within a LunaGUI app).
- LunaGUI is not Electron, Flutter, or GTK. Do not import WebKit, V8, or any browser engine. Do not use GTK or Qt libraries — they are not part of Mahina.
- Widget tree manipulation (adding/removing widgets, changing state) is safe from the main thread only. Do not mutate the widget tree from a background thread.
- For text-heavy custom content (code editors, terminal output), use `LunaCanvas` with the Canvas text rendering API, not a large number of `LunaLabel` widgets. Labels are for UI text; Canvas is for document/terminal text rendering.

---

*Document: `Volume III / 04_lunagui.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 03_compositor.md, living_interface_design.md, core_laws.md (Law III), non_negotiables.md, decision_log.md (DL-004R)*
*Informs: 05_animation_engine.md, 06_theme_engine.md, Volume V (all userland applications)*
