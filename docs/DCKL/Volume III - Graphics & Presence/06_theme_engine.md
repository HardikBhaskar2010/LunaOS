# Mahina — Theme Engine
**Volume III · Chapter 6**
**Classification:** Core Architecture — Visual Identity System
**Status:** Active · Governs all visual appearance within Mahina

---

## Purpose

This document specifies the Mahina Theme Engine: the system that controls the visual appearance of every Mahina surface — colors, typography, spacing, corner radii, shadow styles, and icon sets — while operating strictly within the boundaries of the Color Semantic Contract and the living_interface_design.md principles.

This document is the authoritative reference for:
- The theme data format and schema
- How the Color Resolver maps semantic color codes to actual hex values
- What themes can and cannot change
- The default Mahina theme ("Luna Dark")
- The theme switching mechanism
- Third-party and user-created theme constraints
- The relationship between themes and the locked Color Semantic Contract

---

## Overview

The theme engine answers one question: **what does the Color Semantic Contract look like right now?**

The Color Semantic Contract locks the *meanings* of five colors. It does not permanently lock the *exact hex values*. A theme defines the hex values for each semantic meaning. The default theme defines the canonical hex values documented in `core_laws.md`. A user or community theme may define different hex values — but it cannot reassign a semantic meaning.

**The theme cannot change:**
- The number of semantic colors (always exactly 5)
- The meaning of each semantic color (LUNA Green = healthy, always)
- The Motion Vocabulary (which motion types exist and what they mean)
- The Animation Budget (duration ceilings per class)
- The Z-order layer system (which surfaces appear above others)

**The theme can change:**
- The exact hex value for each semantic color code
- Typography (typeface, size scale, weight scale)
- Spacing (padding, margin, gap increments)
- Corner radii (how rounded or sharp UI elements appear)
- Shadow style (shadow color, blur, spread)
- Widget appearance variants (outlined vs. filled buttons, etc.)
- Icon set (which icon glyphs are used for system icons)
- Background colors and gradients
- LUNA Island appearance (color of the Island background, indicator dots)

---

## Design Philosophy

### Themes express a visual identity, not a new design system

A Mahina theme is not a complete re-skin that can make Mahina look like Windows or macOS. It is a surface-level visual identity that works within Mahina's design vocabulary. The structural elements — which surfaces exist, which ones appear on top, which motions are used — are determined by the OS, not the theme.

This is a deliberate constraint. Mahina's "presence" identity comes partly from visual consistency. A theme that completely replaces the design system undermines the consistency that makes LUNA's expressions meaningful.

### The default theme is the canonical reference

The default theme ("Luna Dark") defines the canonical hex values that all other Mahina documentation refers to when it says "LUNA Green (#2EFF8A)" etc. If a user applies a different theme, those hex values change on their screen — but the semantic meaning does not.

### Dark mode is the default

Mahina ships dark mode as the default. Light mode is an alternative theme. Per the non_negotiables: "Motion Creates Presence." LUNA's expressions are designed for dark backgrounds — the eye animations, color semantic signals, and particle effects are all tuned for dark surfaces.

```
TODO:
Decision not yet finalized.
Reason: Whether Mahina ships a light mode theme as a built-in option in v1
has not been formally decided. The non-negotiables establish dark as the default.
Light mode is a high-requested feature for users in bright environments.
Must be a Decision Log entry. Recommendation: ship both in v1.
```

---

## Architecture

### Theme Data Format

Mahina themes are defined in TOML files (DL-008 — TOML is the system configuration format):

```toml
# /usr/share/luna/themes/luna-dark/theme.toml
# Official Mahina default theme — Luna Dark

[meta]
name        = "Luna Dark"
author      = "Mahina Project"
version     = "1.0"
base        = "dark"    # "dark" or "light" — affects default contrast assumptions
license     = "MIT"

[colors.semantic]
# Semantic color values — these MUST map to one of the 5 locked semantic codes.
# Hex values may be customized. Semantic meanings are fixed.
healthy    = "#2EFF8A"   # LUNA Green — operational, success
critical   = "#FF2D78"   # LUNA Pink — error, danger, critical state
warning    = "#FFB800"   # LUNA Amber — warning, degraded
neutral    = "#F0F0FF"   # LUNA White — idle, information
void       = "#1A1A2E"   # LUNA Void — offline, inactive, absence

[colors.background]
primary    = "#0D0D1A"   # Main application background
secondary  = "#12121F"   # Slightly lighter surfaces (cards, panels)
tertiary   = "#1A1A2E"   # Even lighter surfaces (popovers, dropdowns)
overlay    = "#000000CC" # Semi-transparent overlay for modals

[colors.text]
primary    = "#F0F0FF"   # Primary text color
secondary  = "#B0B0CC"   # Secondary, subdued text
disabled   = "#505070"   # Disabled state text
inverse    = "#0D0D1A"   # Text on light/semantic backgrounds

[colors.border]
default    = "#252540"   # Default border color
focus      = "#2EFF8A"   # Focus ring color (uses semantic healthy)
subtle     = "#1A1A30"   # Very subtle separator

[typography]
# Font family — resolved from system font path
family     = "Inter"     # TODO: pending Default Typeface Decision Log entry
# Size scale (px, assuming 96 DPI — scaled by compositor for HiDPI)
size_xs    = 11
size_sm    = 13
size_md    = 15          # Body default
size_lg    = 18
size_xl    = 22
size_xxl   = 28
size_hero  = 36
# Weight scale
weight_regular  = 400
weight_medium   = 500
weight_semibold = 600
weight_bold     = 700

[spacing]
# Base spacing unit: 4px. All spacing values are multiples of this unit.
unit  = 4
xs    = 4    # 1 unit
sm    = 8    # 2 units
md    = 12   # 3 units
lg    = 16   # 4 units
xl    = 24   # 6 units
xxl   = 32   # 8 units
page  = 48   # 12 units — outer page margins

[shape]
# Corner radius in pixels
radius_sm   = 4    # Small buttons, tags
radius_md   = 8    # Cards, panels, dialogs
radius_lg   = 12   # Large containers
radius_full = 9999 # Fully rounded (pills, toggles)

[shadow]
color        = "#00000080"   # Shadow base color (50% opacity black)
sm_blur      = 4
sm_spread    = 0
sm_offset_y  = 2
md_blur      = 12
md_spread    = 0
md_offset_y  = 4
lg_blur      = 24
lg_spread    = 2
lg_offset_y  = 8

[luna_island]
background        = "#0D0D1A"   # Island background color
indicator_active  = "#2EFF8A"   # Active mode indicator dot
indicator_idle    = "#505070"   # Idle indicator dot
border_color      = "#252540"   # Island border

[icons]
# Icon set — path to icon theme directory
set = "/usr/share/luna/icons/luna-default/"
```

### Theme Directory Structure

```
/usr/share/luna/themes/
└── luna-dark/
    ├── theme.toml          # Theme definition
    ├── icons/              # Optional icon override (links to icon set or local icons)
    └── preview.png         # 256x256 preview image for theme picker UI

~/.luna/themes/
└── my-custom-theme/
    ├── theme.toml
    └── preview.png
```

System themes live in `/usr/share/luna/themes/`. User themes live in `~/.luna/themes/`. User themes do not require `lpkg` — they are installed by copying files.

### Color Resolver

The Color Resolver is a compositor component. It maintains the currently-active theme's color table in memory. When a surface sends `LGP_SET_SEMANTIC_COLOR(LUNA_GREEN)`, the resolver looks up the current theme's `colors.semantic.healthy` value and applies it as the actual rendered color.

The Color Resolver is updated when the user changes themes. All surfaces that use semantic colors immediately reflect the new colors after a theme change — no application restart required.

```c
// Color resolver lookup (compositor internal)
uint32_t color_resolve(uint32_t semantic_code, lgp_theme_t *theme) {
    switch (semantic_code) {
        case LUNA_GREEN:  return theme->colors.semantic.healthy;
        case LUNA_PINK:   return theme->colors.semantic.critical;
        case LUNA_AMBER:  return theme->colors.semantic.warning;
        case LUNA_WHITE:  return theme->colors.semantic.neutral;
        case LUNA_VOID:   return theme->colors.semantic.void_color;
        default:
            // Invalid code — should never reach here (caught at message receipt)
            luna_log(LOG_ERROR, "lgp-compositor", "Invalid semantic code: %u", semantic_code);
            return 0xFF00FF; // Magenta — developer error indicator
    }
}
```

### Theme Loading and Switching

**At compositor startup:**
1. Read `~/.luna/config/theme.toml` to determine the active theme name
2. Load the theme TOML from the appropriate theme directory (user or system)
3. Validate: all required fields present, semantic colors are valid hex, etc.
4. If theme is invalid or missing: fall back to the built-in Luna Dark defaults (hardcoded in compositor)
5. Populate Color Resolver with theme values

**At theme switch (user changes theme in settings):**
1. Settings sends a theme change request via D-Bus or LGP (TBD)
2. Compositor loads the new theme TOML
3. Validates the new theme
4. If valid: atomically updates Color Resolver table
5. All semantic-color surfaces are re-composited on next frame with new colors
6. Compositor notifies all LunaGUI clients: `LGP_THEME_CHANGED` event (so toolkit can update typography, spacing, etc.)
7. LunaGUI applications re-layout and re-render their widget trees with new theme values

```
TODO:
Decision not yet finalized.
Reason: The theme switch notification protocol has not been specified.
How does the compositor notify LunaGUI clients of a theme change?
Options:
  A: LGP protocol message: LGP_THEME_CHANGED (broadcast to all clients)
  B: D-Bus signal: org.mahina.theme.changed
  C: Both A and B (belt and suspenders)
Option C is recommended: LGP for graphical clients, D-Bus for non-graphical
services that need to react to theme changes.
Must be a Decision Log entry.
```

### LunaGUI Theme Integration

LunaGUI widgets read theme values via a `LunaTheme` context object. This object is populated from the active theme at startup and updated on `LGP_THEME_CHANGED` events.

```
// LunaGUI theme API (pseudo-code)
let theme = LunaTheme.current()

button.background_color = theme.colors.semantic.healthy   // LUNA Green
label.font_size = theme.typography.size_md                // 15px
container.padding = theme.spacing.lg                      // 16px
card.corner_radius = theme.shape.radius_md                // 8px
```

Application code never hardcodes hex values or pixel sizes for theme-aware properties. It always reads from `LunaTheme.current()`. This ensures theme compliance without developer effort.

---

## Technical Details

### Default Theme Values

The "Luna Dark" theme defines the canonical values used throughout the DCKL. These are the values that all Volume I–IV documents reference:

| Semantic name | Color name | Hex value | Meaning |
|---|---|---|---|
| `healthy` | LUNA Green | `#2EFF8A` | Operational, success, running |
| `critical` | LUNA Pink | `#FF2D78` | Error, danger, critical failure |
| `warning` | LUNA Amber | `#FFB800` | Warning, degraded, attention needed |
| `neutral` | LUNA White | `#F0F0FF` | Idle, information, neutral state |
| `void` | LUNA Void | `#1A1A2E` | Offline, inactive, absent |

**Background colors:**

| Token | Hex | Use |
|---|---|---|
| `background.primary` | `#0D0D1A` | Main desktop, application backgrounds |
| `background.secondary` | `#12121F` | Cards, sidebars, elevated panels |
| `background.tertiary` | `#1A1A2E` | Popovers, dropdowns, tooltips |
| `background.overlay` | `#000000CC` | Modal dialog backdrop |

**Typography scale (Inter, 96 DPI baseline):**

| Token | Size | Use |
|---|---|---|
| `size_xs` | 11px | Labels, tags, metadata |
| `size_sm` | 13px | Secondary text, captions |
| `size_md` | 15px | Body text (default) |
| `size_lg` | 18px | Subheadings |
| `size_xl` | 22px | Section headings |
| `size_xxl` | 28px | Page headings |
| `size_hero` | 36px | Hero text, splash headings |

### Theme Validation Rules

The theme loader validates the following before accepting a theme:

1. All required TOML sections are present: `meta`, `colors.semantic`, `colors.background`, `typography`, `spacing`, `shape`
2. All 5 semantic colors are present and are valid hex strings
3. Font family name is not empty (actual font availability is checked separately)
4. Size values are positive integers
5. Spacing unit is a positive integer (all spacing values must be multiples of unit)
6. Corner radius values are non-negative
7. Shadow blur values are non-negative

Themes that fail validation are rejected and the fallback (Luna Dark built-in) is used. A WARN log entry records the validation failure.

### Icon Theme

The icon theme is a directory of SVG or PNG files named by icon ID. LunaGUI resolves icon names to file paths via the icon theme directory.

```
/usr/share/luna/icons/luna-default/
├── 16/
│   ├── app-settings.svg
│   ├── app-terminal.svg
│   └── ...
├── 24/
│   └── ...
├── 32/
│   └── ...
└── 48/
    └── ...
```

Icon naming follows a flat namespace: `{category}-{name}`. Size variants are auto-selected based on the display size request. SVG icons are preferred (resolution-independent).

```
TODO:
Decision not yet finalized.
Reason: The Mahina icon set has not been created. A v1 system needs at minimum:
  - Application icons for built-in applications
  - System icons (close, minimize, settings, wifi, battery, notification, etc.)
  - File type icons
  - Status icons (health states, network states, etc.)
Whether Mahina creates original icons or adapts an existing open-source
icon set (e.g., Fluent UI icons, Phosphor icons) has not been decided.
Original icons are preferred for visual identity. Must be a design Decision.
```

---

## Third-Party Theme Constraints

Community and user themes must comply with the following rules to be accepted by the theme engine:

**Enforced rules (validation prevents non-compliance):**
- All 5 semantic color slots must be populated
- The semantic color names (healthy, critical, warning, neutral, void) cannot be renamed
- Required TOML sections cannot be omitted

**Non-enforced rules (guidelines for community themes):**
- Semantic colors should maintain semantic legibility: `critical` (LUNA Pink) should be a clearly alarming color in the chosen palette. A theme where `critical` is light gray fails the spirit of the contract even if it passes validation.
- Dark/light base declaration should match the actual luminosity of `background.primary`
- Typography scale ratios should be preserved even if absolute sizes change

**What themes are explicitly NOT allowed to do (enforced by the OS, not the validator):**
- Replace the Motion Vocabulary or Animation Budget (these are not theme properties)
- Change which surfaces appear in which Z-order layers
- Override the compositor's input routing behavior
- Change LUNA's personality or mode behavior

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Light mode built-in theme | v1 | Pending Decision Log entry |
| Theme marketplace / community themes | v1.5 | Requires lpkg packaging format for themes |
| Per-application theme override | v2 | Allow applications to request a specific theme variant |
| HDR color support in themes | v2 | P3/Rec.2020 color gamut for HDR displays |
| Dynamic theme (time-based) | v1.5 | Automatically switch light/dark based on time of day |
| LUNA-suggested theme changes | v2 | LUNA recommends a theme based on workload/environment |
| Color accessibility validation | v1.5 | Validate contrast ratios against WCAG 2.1 standards |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Light mode.** Built-in light theme in v1? Must be a Decision Log entry.

2. **Theme switch notification protocol.** LGP message vs. D-Bus signal. Option C (both) is recommended. Must be a Decision Log entry.

3. **Default typeface.** Inter / Noto Sans / IBM Plex Sans. Must be a Decision Log entry (shared with `04_lunagui.md` Open Question 2).

4. **Icon set.** Original Mahina icons vs. adapted open-source set. Must be a design decision before v1 UI development begins.

5. **User theme directory.** `~/.luna/themes/` is proposed. Consistent with the `~/.luna/` user data pattern. Confirm this is the right location.

6. **Theme validation severity.** Should a partially valid theme (e.g., missing `luna_island` section) be rejected entirely, or applied with defaults for the missing section? Current answer: reject entirely and fall back to Luna Dark. This is strict but safe.

7. **LUNA Island theming.** The `[luna_island]` section allows theming of Luna Island's appearance. Does this extend to the animation speeds? No — animation speeds are determined by the Animation Budget (locked). Only visual properties (colors, background) are theme-controlled.

---

## AI Context

An AI agent implementing UI components for Mahina must understand:

- Never hardcode hex color values in UI code. Always use `LunaTheme.current().colors.*` for themed colors, or `LGP_SET_SEMANTIC_COLOR` semantic codes for protocol-level color declarations.
- The five semantic colors have fixed meanings. LUNA Green = healthy/success. LUNA Pink = critical/danger. LUNA Amber = warning. LUNA White = neutral. LUNA Void = inactive. Do not use LUNA Green for decorative purposes — it communicates operational status.
- `LunaTheme.current()` returns the live theme context. Read it at render time, not at construction time, so that theme changes are reflected without restarting.
- Icon names follow the `{category}-{name}` convention. Use `LunaIcon.resolve("app-settings", size: 24)` to get the correct icon path for the active icon theme. Do not hardcode paths.
- If the theme engine falls back to Luna Dark built-in defaults, it logs at WARN. Watch for these log entries during development — they indicate a theme TOML validation failure.
- Themes control visual appearance only. If you need to change compositor behavior, motion behavior, or Z-order, that requires a code change, not a theme change.
- The `colors.semantic.critical` value is NOT a UI accent color. Do not use it for emphasis or to draw attention to non-critical states. It is specifically for errors, failures, and dangerous conditions.

---

*Document: `Volume III / 06_theme_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 03_compositor.md, 04_lunagui.md, core_laws.md (Law III — Color Semantic Contract), living_interface_design.md, decision_log.md (DL-008)*
*Informs: Volume IV (Luna Island theming), Volume V (Settings / theme picker application)*
