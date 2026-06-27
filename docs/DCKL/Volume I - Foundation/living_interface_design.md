# Mahina — Living Interface Design
**Volume I · Chapter 7**
**Classification:** Foundation Document — Design Principle
**Status:** Canonical · Implementation deferred to Volume III

---

## Purpose

This document defines **Living Interface** as a design discipline: the contract that every visual surface in Mahina — not only LUNA — must satisfy in order to communicate real system state through motion, color, and form.

This document does **not** specify:

- Rendering pipeline implementation
- Compositor internals
- Wire protocol details for the Luna Graphics Protocol (LGP)
- Theme Engine implementation
- Animation Engine implementation

Those belong to **Volume III — Graphics & LGP** and are explicitly out of scope here. This document exists so that Volume III has a conceptual contract to implement against, and so that no Volume III document needs to re-derive *why* a given color, motion, or timing rule exists.

This document is the bridge between:

- `00_Foundation/philosophy.md` — Law III, "Aesthetic Is Functional"
- `00_Foundation/core_laws.md` — Law III, the Animation Budget, the Color Semantic Contract, the Motion Vocabulary
- `00_Foundation/luna_personality.md` — the Expression Layer priority hierarchy
- `00_Foundation/identity.md` — the Luna Island component

...and the not-yet-written Volume III documents that will implement these rules in code.

---

## Overview

Mahina's founding premise, per `vision.md`, is **presence instead of features**. A Living Interface is the mechanism by which "presence" becomes observable rather than aspirational.

A surface is "living" when:

1. Every animation on it corresponds to a real, current system state (Law III, `core_laws.md`)
2. Every color used on it obeys the locked Color Semantic Contract
3. Its motion vocabulary is drawn only from the locked Motion Vocabulary table
4. Communication is attempted at the lowest-disruption priority level first (Expression Layer, `luna_personality.md`)

Critically: **Living Interface is not a synonym for LUNA.** LUNA is the most expressive instantiation of this principle — she has eyes, a Live2D model, dialogue, and the full Luna Island surface available to her. But the principle itself applies system-wide. A window border, a notification card, a progress indicator, a settings toggle — all of these are surfaces that should obey the same contract, even though none of them are "LUNA."

```
                    ┌─────────────────────────┐
                    │   "Presence instead     │
                    │    of features"         │
                    │   (vision.md)           │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │  Law III — Aesthetic    │
                    │  Is Functional          │
                    │  (philosophy.md /       │
                    │   core_laws.md)         │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   LIVING INTERFACE       │
                    │   (this document)        │
                    └────────────┬────────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              ▼                  ▼                  ▼
      Luna Island         Other system        Application
      (identity.md —      chrome (TODO —      surfaces
      already speced)     Volume III)         (TODO — open
                                               question, see
                                               below)
```

---

## Design Philosophy

### Decoration is rejected by default

Per Law III (`philosophy.md`, `core_laws.md`): *"If it's purely decorative, don't ship it. If it communicates state, ship it."* Living Interface inherits this rule without modification. No Volume III implementation document may introduce a visual flourish that does not map to a real state.

### LUNA is an instance, not the whole system

`luna_personality.md` defines an Expression Layer priority order for LUNA specifically:

```
Priority 1: Eye direction / gaze
Priority 2: Accent color shift
Priority 3: Animation type change
Priority 4: Luna Island expansion/contraction
Priority 5: Prop appearance (if Live2D enabled)
Priority 6: Text notification
Priority 7: Spoken dialogue (if voice enabled)
```

Living Interface generalizes this into a system-wide rule:

> **Every surface should attempt to communicate state at the lowest-numbered priority level it has available, before escalating.**

A surface without eyes or a Live2D model obviously cannot use Priority 1. But it still has a priority floor — typically color (Priority 2) or motion (Priority 3) — and should exhaust that floor before falling back to text. A notification card that immediately shows a paragraph of text when a color shift would have sufficed is not following Living Interface, even though it isn't "LUNA's" surface.

### The Animation Budget is a system-wide constraint, not a LUNA-specific one

`core_laws.md` locks these durations:

| Interaction type | Max duration |
|---|---|
| Button press / click response | ≤ 100ms |
| UI element transition | ≤ 200ms |
| Window open / close | ≤ 300ms |
| State change animation | ≤ 400ms |
| Complex narrative motion (boot splash) | ≤ 2000ms |

Living Interface treats this table as binding on **every** animated element in Mahina, not only Luna Island. Volume III implementation documents (Compositor, Animation Engine, Theme Engine) must cite this table rather than redefine it.

### The Color Semantic Contract and Motion Vocabulary are closed sets

Both tables in `core_laws.md` (Color Semantic Contract; Motion Vocabulary) are explicitly **locked**. Living Interface does not add to either table. Any Volume III document that needs a new color or motion type must trigger the amendment process defined in `core_laws.md` → Amendments, before that color or motion may be specified anywhere, including in this document's own future revisions.

---

## Architecture

This document defines a conceptual layer, not a runtime component. The layer sits between the constitutional rules (Core Laws) and the concrete implementation documents of Volume III.

```
┌────────────────────────────────────────────────────┐
│                 LIVING INTERFACE                     │
│            (design discipline — this doc)            │
├──────────────────────────────────────────────────────┤
│  Governs (already locked, cited not redefined):       │
│   - Color Semantic Contract   (core_laws.md §III)     │
│   - Motion Vocabulary          (core_laws.md §III)     │
│   - Animation Budget           (core_laws.md §III)     │
│   - Expression Priority Order  (luna_personality.md)   │
├──────────────────────────────────────────────────────┤
│  Implemented by (Volume III — status: TODO):          │
│   - Luna Graphics Protocol (LGP)                       │
│   - Compositor                                          │
│   - Rendering Pipeline                                  │
│   - Animation Engine                                    │
│   - Theme Engine                                         │
├──────────────────────────────────────────────────────┤
│  Already partially specified (Volume I — identity.md): │
│   - Luna Island (states, dimensions, transition timing, │
│     zwlr_layer_shell_v1)                                 │
└────────────────────────────────────────────────────┘
```

Luna Island is the only surface in the project that currently has a concrete technical spec (`identity.md` → "Luna Island — Component Spec" / "Technical Notes"). It should be treated as the **reference implementation** of Living Interface principles once Volume III documents exist — Volume III documents should show how their primitives produce Luna Island's documented behavior, not the reverse.

No other UI surface (window chrome, notification cards outside Luna Island, settings UI, application-level chrome) currently has an architecture. Per project rule, this document does not invent one. See **Open Questions**.

---

## Current Decisions

This document makes no new architectural decisions. The following are **inherited** decisions, restated here only to establish that Living Interface does not override them:

| Decision | Source | Status |
|---|---|---|
| Color Semantic Contract (5 colors, fixed meanings) | `core_laws.md` Law III | Locked |
| Motion Vocabulary (9 motion types, fixed meanings) | `core_laws.md` Law III | Locked |
| Animation Budget (duration ceilings per interaction class) | `core_laws.md` Law III | Locked |
| Expression Layer priority order (1–7) | `luna_personality.md` | Locked (LUNA-specific; generalized here) |
| Luna Island is the canonical surface for LUNA's expression | `identity.md` | Locked |
| New motion types require an amendment before use anywhere | `core_laws.md` Amendments | Locked |
| Compositor strategy: Full LGP (no Wayland/Hyprland) | `decision_log.md` DL-004R | Accepted (bounds what Volume III can implement in v1) |

---

## Technical Details

Intentionally minimal. Living Interface is a design contract, not an implementation. Concrete technical decisions belong to Volume III documents once they exist:

- **Luna Graphics Protocol (LGP):** TODO — not yet specified. Should be written to expose the Color Semantic Contract, Motion Vocabulary, and Animation Budget as protocol-level primitives rather than per-application conventions.
- **Compositor:** Per DL-004R, the LGP compositor is used from v1. The animation system will be a custom component. No decision has been made on how Living Interface's locked tables map onto the LGP animation config syntax. TODO.
- **Rendering Pipeline / Animation Engine / Theme Engine:** TODO — no decisions exist yet. Decision not yet finalized.
- **Frame budget / refresh-rate assumptions:** TODO — not addressed in any canonical document.
- **GPU / hardware minimum requirements for Living Interface motion:** TODO.

---

## Future Improvements

- The LGP compositor (per DL-004R) ensures that Living Interface primitives are native to the compositor rather than expressed through configuration files.
- Volume III's Theme Engine document should define how third-party or user-created themes are constrained by the locked Color Semantic Contract (i.e., can a user retheme LUNA Pink to a different hex value, or is the *meaning* locked but the *exact hex* themeable?). Decision not yet finalized.
- Audio is not currently part of Living Interface. If DL-P02 (Sound design, `decision_log.md` Pending Decisions) is accepted, this document will need a revision to define an audio-equivalent of the Motion Vocabulary.

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

Specifically unresolved:

1. **Scope of enforcement.** Does Living Interface apply only to Mahina-native chrome (Luna Island, system notifications, system-level surfaces), or does it extend to third-party application windows running on Mahina? No canonical document currently answers this.
2. **Accessibility / reduced-motion mode.** The Motion Vocabulary and Animation Budget are locked under Law III. It is not yet decided how (or whether) a reduced-motion accessibility mode can coexist with a "locked" vocabulary, since disabling motion would mean some states stop being communicated entirely.
3. **Degradation on low-power hardware.** No minimum hardware spec for Living Interface motion has been decided. Related to DL-007 (musl migration, v2) but not addressed by it.
4. **Non-Luna-Island surfaces.** `identity.md` specs Luna Island in detail. No other surface (window borders, generic notification cards, settings app, lock screen) has been specified. Whether these surfaces are in scope for Volume III's first pass or deferred is undecided.

---

## AI Context

This document is a **conceptual contract**, not a buildable spec. An AI coding agent should not attempt to generate Compositor, LGP, Animation Engine, or Theme Engine code directly from this document.

Before implementing any animated, colored, or state-expressing UI element on Mahina, an AI agent must:

1. Check the element's intended meaning against the Color Semantic Contract and Motion Vocabulary in `00_Foundation/core_laws.md` (Law III). Do not introduce a new color or motion type — if the needed signal doesn't exist in either table, stop and flag it as requiring the amendment process, do not invent one.
2. Check the element's animation duration against the Animation Budget table in the same document. An animation that cannot fit its class's ceiling must be redesigned, not shipped over-budget.
3. Apply the Expression Layer priority order from `00_Foundation/luna_personality.md`: attempt the lowest-numbered priority (color/motion) before falling back to text or dialogue, even for non-LUNA surfaces.
4. Treat Luna Island (`00_Foundation/identity.md` → "Luna Island — Component Spec") as the only currently-specified reference implementation. In the absence of a Volume III document, do not assume any other surface's technical implementation — flag it as a TODO instead.
5. Do not generate LGP, Compositor, Animation Engine, or Theme Engine architecture from this document. Those require their own Volume III documents, none of which currently exist.

---

*Document: `00_Foundation/living_interface_design.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-concept*
*Cross-references: `philosophy.md` (Law III), `core_laws.md` (Law III, Amendments), `luna_personality.md` (Expression Layer), `identity.md` (Luna Island), `decision_log.md` (DL-004R)*
