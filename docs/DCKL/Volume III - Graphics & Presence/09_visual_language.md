# Mahina — Visual Language
**Volume III · Chapter 9**
**Classification:** Core Architecture — Graphics & Presence
**Status:** Canonical · This is the authoritative reference for every visual decision in Mahina

---

## Purpose

This document defines the **Visual Language of Mahina** — the complete vocabulary of how Mahina looks, moves, and communicates through design. It is the single source of truth for:

- The color system and semantic meaning of every color
- Typography: fonts, sizes, weights, and usage rules
- Spacing and density
- Shape language (corner radii, borders, shadows)
- Motion principles and timing vocabulary
- The glass aesthetic (depth, layering, transparency)
- Icons and visual symbols
- The emotional register of visual elements

Without this document, different components will make different visual decisions that produce an incoherent design. This document ensures every component speaks the same visual language.

---

## Overview

Mahina has one visual personality: **precise, present, alive**.

```
Three words that govern every visual decision:

  PRECISE — Nothing is accidental. Every spacing value, corner radius,
             and font weight is intentional. Precision communicates
             craftsmanship and builds trust.

  PRESENT — The interface is aware. It responds. It notices.
             Presence is communicated through motion, context-sensitive
             color, and LUNA's always-visible Island.

  ALIVE   — The interface breathes. Elements animate with natural
             easing. Nothing cuts abruptly. The desktop feels like
             a living system, not a static grid of rectangles.
```

These three words are the test for every visual decision: if a proposed design element is precise, present, and alive, it belongs in Mahina. If it is generic, static, or careless, it does not.

---

## Color System

### Foundation: Dark Environment

Mahina v1 ships **Luna Dark** as its only built-in theme (DL-039). All color decisions are made for dark environments first. The dark environment is not a constraint — it is the designed state.

```
Luna Dark color environment:

  Void Black    #0A0A0F   — Deepest background (void of space)
  Deep Navy     #0D0D1A   — Secondary background layer
  Surface Dark  #12121E   — Card and panel surfaces
  Surface Mid   #1A1A2E   — Elevated surfaces (hover states)
  Surface High  #22223A   — Highest surface (tooltips, modals)
  Border Dim    #2A2A4A   — Subtle borders, dividers
  Border Active #3D3D6B   — Active / focused borders
```

### Semantic Colors

These six semantic colors carry **fixed meanings** that cannot be changed by themes. Themes may change their hex values, but never their meaning.

```
Semantic Color Contract:

  LUNA_GREEN  (#00E5A0) — Operational. Healthy. Success. Active.
                          "This thing is working correctly."

  LUNA_PINK   (#FF3CAC) — Critical. Error. Danger. Alert.
                          "This needs immediate attention."

  LUNA_AMBER  (#FFB347) — Warning. Degraded. Caution. Approaching limit.
                          "This is not optimal but not broken."

  LUNA_WHITE  (#E8E8FF) — Neutral. Information. Idle. Passive.
                          "Here is data, with no judgment attached."

  LUNA_VOID   (#3A3A5C) — Absent. Offline. Inactive. Disconnected.
                          "This thing is not here right now."

  LUNA_BLUE   (#4A9EFF) — Interactive. Selectable. Actionable.
                          "This can be clicked / touched / activated."
```

**Rule:** An application may never use LUNA_GREEN to indicate an error, or LUNA_PINK to indicate success. The semantic meaning is part of the protocol (DL-033). Violating this rule breaks the user's ability to read system state at a glance.

### Text Colors

```
Text color hierarchy:

  Text Primary      #FFFFFF (1.0 opacity) — Headings, important content
  Text Secondary    #FFFFFF (0.7 opacity) — Body text, descriptions
  Text Tertiary     #FFFFFF (0.4 opacity) — Placeholder, metadata, hints
  Text Disabled     #FFFFFF (0.2 opacity) — Disabled controls
  Text Inverse      #0A0A0F              — Text on light surfaces (toasts)
```

### Interactive States

```
State color modifier rules:

  DEFAULT state: base color at full opacity
  HOVER state:   base color + Surface overlay at 8% opacity
  PRESSED state: base color + Surface overlay at 16% opacity
  FOCUSED state: base color + LUNA_BLUE border (2px)
  DISABLED state: all colors at 40% opacity
  LOADING state: animated shimmer overlay
```

---

## Typography System

### Font Hierarchy (DL-028, AP-001)

Mahina uses **two typeface roles**, per the Typography Philosophy Principle:

**Display Role — Bitcount**
Used where personality matters:
- Boot screen
- Login screen
- LUNA Island text
- LUNA responses in conversation
- Application names in the dock
- Major headings (h1, h2)
- Modal dialog titles

**Reading Role — TBD (companion font pending DL entry)**
Used where readability matters:
- Body text in applications
- Documentation
- Terminal emulator (monospace variant)
- Code editors
- Settings panels
- Small labels and metadata

Until the companion font is selected, the interim fallback is **Inter** (available as a system fallback on most Linux systems, open license, screen-optimized).

### Type Scale

```
Type scale (in pixels at 1x density):

  Display Large   — 48px, weight 700, Bitcount, tracking: -0.5px
  Display Medium  — 36px, weight 700, Bitcount, tracking: -0.3px
  Heading 1       — 28px, weight 600, Bitcount, tracking: -0.2px
  Heading 2       — 22px, weight 600, Reading,  tracking: 0
  Heading 3       — 18px, weight 600, Reading,  tracking: 0
  Body Large      — 16px, weight 400, Reading,  tracking: 0
  Body            — 14px, weight 400, Reading,  tracking: 0
  Body Small      — 12px, weight 400, Reading,  tracking: 0.1px
  Label           — 11px, weight 500, Reading,  tracking: 0.3px
  Caption         — 10px, weight 400, Reading,  tracking: 0.4px
  Code            — 13px, weight 400, Monospace,tracking: 0

Minimum text size: 10px — below this, text is illegible at standard DPI
```

### Line Height

```
Line height rules:

  Display      — 1.1× font size
  Heading      — 1.2× font size
  Body         — 1.5× font size (generous for readability)
  Label        — 1.3× font size
  Code         — 1.6× font size (code needs breathing room)
```

---

## Spacing System

Mahina uses a **4px base grid**. All spacing values are multiples of 4.

```
Spacing scale:

  space-1  =  4px   — micro gap (icon to label, within a row)
  space-2  =  8px   — small gap (between related items in a list)
  space-3  = 12px   — inner padding (inside buttons, chips)
  space-4  = 16px   — default padding (card inner padding)
  space-5  = 20px   — medium gap (between card sections)
  space-6  = 24px   — screen margin (Luna Island margin from edge)
  space-8  = 32px   — large gap (between unrelated sections)
  space-10 = 40px   — xlarge gap (page section breaks)
  space-12 = 48px   — 2xlarge (hero section padding)
  space-16 = 64px   — dock height, large structural elements
```

**Rule:** Never use a spacing value not in this scale. Arbitrary pixel values (17px, 23px, 11px) are forbidden in production code. If a spacing value feels wrong at the nearest scale value, the layout assumption is wrong — not the scale.

---

## Shape Language

### Corner Radius System

```
Corner radius scale:

  radius-0  =  0px  — No rounding (dividers, separators, pixel-art elements)
  radius-1  =  4px  — Subtle rounding (input fields, small chips)
  radius-2  =  8px  — Standard rounding (cards, panels, buttons)
  radius-3  = 12px  — Medium rounding (modal dialogs)
  radius-4  = 16px  — Large rounding (notification toasts)
  radius-5  = 20px  — XL rounding (Luna Island COMPACT_PANEL)
  radius-6  = 24px  — Shell panels, bottom sheet
  radius-full = 9999px — Full pill shape (Luna Island AMBIENT dot,
                          toggle switches, circular buttons)
```

**Principle:** More important, more prominent elements have larger corner radii. Luna Island AMBIENT is a perfect circle (radius-full). Application windows use radius-2. The screen edges themselves have no rounding.

### Border Rules

```
Border usage:

  No border:      — Surfaces on the deepest background layer
  border-dim:     — Cards and panels (1px, #2A2A4A)
  border-active:  — Focused or hovered elements (1px, #3D3D6B)
  border-semantic:— Semantic-colored borders (status indicators)

  Border width: always 1px in v1. No 2px borders except focus rings.
  Focus ring: 2px LUNA_BLUE border, 1px gap from element edge.
```

### Shadows and Elevation

```
Elevation system:

  Elevation 0 — No shadow (base background surfaces)
  Elevation 1 — box-shadow: 0 2px 8px rgba(0,0,0,0.4)    (cards)
  Elevation 2 — box-shadow: 0 4px 16px rgba(0,0,0,0.5)   (panels, dropdowns)
  Elevation 3 — box-shadow: 0 8px 32px rgba(0,0,0,0.6)   (modals)
  Elevation 4 — box-shadow: 0 16px 48px rgba(0,0,0,0.7)  (full-screen overlays)

  Shadow color is always pure black (#000000) at varying opacity.
  Never use colored shadows in v1.
```

---

## Glass Aesthetic

The Luna Dark visual identity is built on **depth through transparency** — the glassmorphism aesthetic adapted for a dark, space-like environment.

```
Glass effect specification:

  Background blur:     backdrop-filter: blur(20px)
  Background tint:     rgba(13, 13, 26, 0.75)  — dark navy at 75% opacity
  Border:              1px solid rgba(255, 255, 255, 0.08)
  Inner highlight:     1px solid rgba(255, 255, 255, 0.04) on top edge only
  Shadow:              Elevation 1–3 depending on surface

Visual result: surfaces appear to float over the content beneath them,
               with depth communicated by blur intensity and shadow size.
```

```
Glass layer stack (from bottom to top):

  ▼ Content / wallpaper
  ▼ Blur layer (20px gaussian blur of content below)
  ▼ Tint layer (dark navy at 75% opacity)
  ▼ Surface layer (card content: text, icons, widgets)
  ▼ Border layer (1px rgba border)
  ▼ Highlight layer (top-edge inner glow, 1px)
```

**Performance note:** Backdrop blur is GPU-intensive. In v1 (software renderer), backdrop blur is **disabled** — surfaces use the tint layer only, with no blur. Backdrop blur is enabled when the Vulkan backend is active (Stage 3+). The design must look acceptable without blur in Stage 2 development.

---

## Motion Principles

The motion language of Mahina is fully specified in `Volume III / 05_animation_engine.md`. This section provides the **visual design rationale** behind the motion rules.

### Three Motion Rules

**1. Motion has meaning.**
Nothing animates for decoration alone. Every animation communicates information: state change, spatial relationship, processing activity, or emphasis. An animation that communicates nothing is a distraction.

**2. Motion respects priority.**
User-initiated actions animate faster than system-initiated ones. A button press should feel instant (< 150ms). A background status update can take longer (up to 600ms). The user's action is always the fastest thing on screen.

**3. Motion is natural.**
All easing functions approximate natural physical motion. Things that start from rest ease in. Things that stop ease out. Things that spring back overshoot slightly. The `ease-out-quint` curve is the primary Mahina easing — fast to appear, smooth to settle.

### Timing Reference

```
Timing vocabulary:

  Instant   — 0–100ms   — Feedback (button press darkening)
  Fast      — 100–200ms — UI responses (menu open, toggle)
  Standard  — 200–350ms — State transitions (panel expansion)
  Deliberate— 350–600ms — Contextual shifts (mode change, scene transition)
  Ambient   — 600ms–4s  — Background presence (Luna Island breathing)

  Nothing user-initiated should ever take longer than 350ms to complete
  its first frame of animation. The user must see a response to their
  action within 100ms.
```

---

## Icons and Symbols

### Icon Style

Mahina icons follow these rules:

```
Icon design rules:

  Grid:         24×24px base grid (scalable to 16/20/32/48)
  Stroke:       2px uniform stroke weight (no fill-only icons)
  Corner style: Rounded — matching radius-1 (4px at 24px icon size)
  Style:        Line icons, not filled/solid
  Optical center: vertically centered within the grid, not mathematically
```

**Icon set:** v1 uses a curated custom icon set for system UI (luna-bar, luna-shell, settings). The icon set is not finalized — pending a design decision. No external icon font (Font Awesome, Material Icons) is used — these carry visual identities that conflict with Mahina's precision aesthetic.

```
TODO:
Decision not yet finalized.
Reason: Mahina icon set not selected / created.
Options:
  A: Commission original icon set (best visual identity, requires design work)
  B: Adapt an existing open-source set (Phosphor Icons, Lucide) as a starting point
  C: Use Unicode symbols for v1 (acceptable for development, not for release)
Recommendation: Option B — start with Phosphor Icons (open source, MIT, line-style,
                consistent with Mahina aesthetic) and customize as needed.
Must be a Decision Log entry before Stage 3.
```

### System Symbols

These Mahina-specific symbols are defined here and used throughout the UI:

| Symbol | Meaning | Usage |
|---|---|---|
| ● (filled circle) | Presence / alive | Luna Island AMBIENT dot |
| ○ (empty circle) | Absent / offline | Luna Island VOID state |
| ▌ (text cursor) | LUNA typing / streaming | Conversation panel |
| 🎤 | Voice input available | Conversation panel (TTS enabled) |
| ↗ | Detach / expand to window | Conversation panel header |
| × | Dismiss / close | Panel close buttons |
| ~ | Ambient wave / breathing | Motion representation in diagrams |

---

## Density Modes

Mahina supports two density modes:

```
Density modes:

  COMPACT  — Default. Optimized for productivity.
             Spacing reduced by 25% from the base scale.
             Smaller font sizes at tertiary levels.
             More content fits on screen.

  REGULAR  — Relaxed. Better for touch / accessibility.
             Full spacing scale as defined above.
             Larger touch targets (minimum 44×44px).
```

The active density mode is stored in `~/.luna/config/display.toml` and read by LunaGUI at startup. Changing density restarts the layout engine — there is no live density switching in v1.

---

## Accessibility Contrast Requirements

All text colors must meet **WCAG AA contrast ratio** requirements against their background:

```
Contrast requirements:

  Text Primary   on Void Black   — 21:1  (white on near-black — exceeds AAA)
  Text Secondary on Void Black   — 10:1  (0.7 opacity white — exceeds AA)
  Text Tertiary  on Void Black   — 4.5:1 (0.4 opacity — meets AA minimum)
  Text Disabled  on Void Black   — 1.8:1 (intentionally below — disabled)

  LUNA_BLUE (#4A9EFF) on Void Black — 6.2:1 (exceeds AA for interactive elements)
  LUNA_GREEN (#00E5A0) on Void Black — 7.8:1 (exceeds AA)
  LUNA_PINK (#FF3CAC) on Void Black — 5.1:1 (exceeds AA)
  LUNA_AMBER (#FFB347) on Void Black — 7.0:1 (exceeds AA)
```

**Rule:** No text or interactive element may ever fall below 4.5:1 contrast ratio against its background. The contrast checker runs as part of the theme validation tool (v1.5).

---

## Anti-Patterns

These visual patterns are explicitly **forbidden** in Mahina:

| Anti-Pattern | Why Forbidden |
|---|---|
| Plain white background (`#FFFFFF`) | Contradicts the dark identity, creates jarring contrast |
| Pure black background (`#000000`) | Too harsh — use Void Black (`#0A0A0F`) instead |
| Colored shadows | Shadows are always black — colored glow is a separate effect |
| Arbitrary spacing (not on the 4px grid) | Breaks the precision aesthetic |
| Semantic color misuse (green for error) | Breaks the semantic color contract (DL-033) |
| Non-Bitcount font for display text | Breaks the typographic personality |
| More than 3 font sizes in a single view | Creates visual chaos |
| Animations without easing (linear) | Feels mechanical and harsh |
| Animations > 600ms that block user input | Never block interaction for an animation |
| Drop shadows without matching elevation | Shadows must match the elevation system |
| Gradients with more than 2 color stops | Complex gradients look cheap — use 2 stops max |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Luna Dark only in v1 | DL-039 | ✅ Accepted |
| Bitcount as display font | DL-028 | ✅ Accepted |
| FreeType + HarfBuzz for text rendering | DL-029 | ✅ Accepted |
| Semantic color contract (6 colors) | non_negotiables.md, DL-033 | ✅ Accepted |
| 4px base grid | This document | ✅ Accepted |
| Backdrop blur disabled in Stage 2 (software renderer) | DL-026 | ✅ Accepted |
| Typography Philosophy (Personality / Readability split) | AP-001 | ✅ Accepted |
| Companion reading font | Pending DL entry | 🔵 Draft |
| Icon set selection | Pending DL entry | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Companion reading font.** The display font is Bitcount. The reading font is unselected. A decision is needed before any productivity application UI is designed. Recommendation: Evaluate IBM Plex Sans, Inter, or commission a companion typeface.

2. **Icon set.** No icon set selected. Recommendation: Phosphor Icons as a starting point, customized. Must be a Decision Log entry.

3. **HiDPI scaling.** This document describes a 1x density system. For 2x HiDPI (Retina-equivalent), all spacing and font values are multiplied. The compositor must report the display's device pixel ratio via `LGP_OUTPUT_INFO` and LunaGUI must scale accordingly. Scaling strategy must be specified before any display work begins.

4. **Light mode color system.** Luna Dark is v1. Luna Light is v2+. However, the semantic color contract must be validated for light backgrounds before the theme engine is extended. Most semantic colors (especially LUNA_PINK) may need different hex values in a light environment. Must be addressed when light mode planning begins.

5. **Motion reduction.** Users with vestibular disorders should be able to disable motion. A `prefers-reduced-motion` equivalent config option in `display.toml` should disable all non-essential animations. Luna Island breathing would collapse to a static dot. Must be designed before Stage 3.

---

## AI Context

- This document is the visual design specification. When writing rendering code for any LunaGUI widget, all spacing values MUST come from the spacing scale. No arbitrary pixel values.
- The semantic colors are a protocol, not suggestions. If a widget needs to communicate "success," it uses LUNA_GREEN. Do not use hex values directly in widget code — use the `LunaTheme.current().color.*` tokens.
- The glass effect (backdrop blur) is disabled in Stage 2. Write the rendering code so that blur is an optional effect layered on top of the tint — not baked into the surface rendering. This allows it to be enabled when the Vulkan backend arrives without a rewrite.
- Bitcount is for display use. Do not use Bitcount for body text, labels, or anything that will be read for more than a few seconds. Use the reading font (or Inter fallback) for those contexts.
- "Alive" means the interface is never completely static. Luna Island breathes. Focused elements have a subtle pulse. However, AMBIENT animations must use ≤ 1% CPU — they must be GPU-composited, not CPU-driven per-frame renders.

---

*Document: `Volume III / 09_visual_language.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 05_animation_engine.md, 06_theme_engine.md, 07_luna_island.md, DL-028, DL-039, AP-001*
*Informs: All Volume V UI components, Volume VI coding standards for UI*
