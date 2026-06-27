# Mahina — Animation Engine
**Volume III · Chapter 5**
**Classification:** Core Architecture — Motion System
**Status:** Active · The animation engine is what makes Mahina feel alive

---

## Purpose

This document specifies the Mahina Animation Engine: the compositor-resident system that drives all motion on screen, enforces the Animation Budget, and translates Motion Vocabulary class declarations into per-frame surface transforms.

This document is the authoritative reference for:
- How the animation engine integrates with the rendering pipeline
- The per-motion-class behavior (easing, duration ceiling, transform set)
- Budget enforcement and auto-completion
- Concurrent animation management
- The relationship between LUNA.AI's Expression Layer and the animation engine
- Performance targets and complexity budgets

---

## Overview

The Animation Engine runs inside the LGP compositor (not as a separate process). It advances once per frame, driven by the compositor's vsync-aligned frame clock. It is the component that makes Mahina feel alive — every surface transition, every LUNA expression change, every notification appearance is driven by the animation engine.

The animation engine has one fundamental constraint it must never violate: **no animation may exceed its class ceiling from the Animation Budget table in `core_laws.md` Law III.** This constraint is enforced by auto-completion — an animation that reaches its ceiling is snapped to its final state in the next frame, regardless of its intended visual arc.

---

## Design Philosophy

### Motion communicates state — it never decorates

Per `living_interface_design.md` and Law III: every animation must map to a real system state change. The animation engine does not have a "decorative animation" type. Every animation object in the engine was created by a `LGP_SEND_MOTION` message, which required a motion class from the locked Motion Vocabulary. If the motion class is not in the vocabulary, the compositor rejected the message before it reached the animation engine.

### The Animation Budget is a hard ceiling, not a guideline

The animation engine tracks elapsed time for every active animation. The moment an animation's elapsed time equals its class ceiling, the engine immediately applies the final transform state and marks the animation complete. There is no grace period, no "please finish" signal. The ceiling is the law.

This design means: **application developers should design animations to finish before the ceiling.** An animation that always hits the ceiling is a visual glitch (it "pops" to its final state rather than transitioning smoothly). The ceiling is a safety rail, not a target.

### Concurrent animations have a complexity budget

Running many animations simultaneously is expensive. The animation engine enforces a **concurrent animation complexity budget** to protect frame timing. When the budget is exceeded, lower-priority animations are deferred to the next frame or dropped.

### LUNA.AI is the primary driver of meaningful animation

Most system animations (window open, button press, notification slide) are brief and mechanical — they follow the Motion Vocabulary table exactly with no creativity. LUNA.AI's expressions (via luna-island) are where the animation engine gets to express personality. The Expression Layer priority order (`luna_personality.md`) determines which LUNA expression animation runs when multiple signals arrive simultaneously.

---

## Architecture

### Integration with the Compositor

```
Compositor render loop (per frame):
  │
  ├── [1] Process LGP messages (may add/remove animations)
  │
  ├── [2] Advance animation engine (one frame)
  │         │
  │         ├── For each active animation:
  │         │     - Compute elapsed time since animation start
  │         │     - If elapsed ≥ class ceiling: mark COMPLETE, apply final state
  │         │     - Else: compute current transform via easing function
  │         │
  │         └── Produce transform table: surface_id → current transform
  │
  ├── [3] Assemble scene (apply transforms from animation engine)
  │
  └── [4] GPU render pass
```

The animation engine outputs a **transform table**: a mapping from surface ID to the current frame's transform (position, scale, opacity, blur). The scene assembler applies these transforms when compositing surfaces.

The animation engine does not call any GPU functions itself. It produces data; the renderer uses it.

### Animation Object

Each active animation is represented internally as:

```c
typedef struct lgp_animation {
    uint32_t       surface_id;      // Surface being animated
    uint32_t       motion_class;    // Locked Motion Vocabulary class
    uint64_t       start_ts;        // Animation start timestamp (ms)
    uint64_t       ceiling_ms;      // Class budget ceiling (ms)
    lgp_transform  from;            // Starting transform state
    lgp_transform  to;              // Target transform state
    lgp_easing     easing;          // Easing function for this class
    bool           complete;        // True when animation has finished
} lgp_animation_t;

typedef struct lgp_transform {
    float  x, y;         // Translation (pixels)
    float  scale_x;      // X scale factor (1.0 = no scale)
    float  scale_y;      // Y scale factor
    float  opacity;      // 0.0 (transparent) to 1.0 (opaque)
    float  blur_radius;  // Gaussian blur radius (0.0 = no blur)
    float  rotation;     // Rotation in degrees (0.0 = no rotation)
} lgp_transform_t;
```

Not all motion classes use all transform properties. For example, `Fade` only modifies `opacity`. `Slide` only modifies `x` or `y`. `Scale` modifies `scale_x` and `scale_y`. The animation engine applies only the relevant properties and leaves others at their baseline values.

### Motion Vocabulary Implementation

The following table specifies the animation engine's behavior for each Motion Vocabulary class:

| Motion Class | Transform properties | Default easing | Budget ceiling | Typical use |
|---|---|---|---|---|
| `Fade` | opacity: 1→0 or 0→1 | Ease-out | 200ms | Element appear/disappear |
| `Slide` | x or y offset: from→0 or 0→from | Ease-out | 200ms | Panel entrance, notification slide |
| `Scale` | scale_x, scale_y: from→1 or 1→from | Spring (ease-out-back) | 200ms | Dialog appear, icon tap |
| `Ripple` | opacity + scale from center point | Ease-out | 100ms | Button press feedback |
| `Pulse` | scale_x, scale_y: 1→1.05→1 (loop) | Ease-in-out (sine) | 400ms per cycle | Indeterminate loading, AI thinking |
| `Expand` | x, y, scale_x, scale_y from origin | Ease-out | 300ms | Window open, panel expand |
| `Collapse` | x, y, scale_x, scale_y to origin | Ease-in | 300ms | Window close, panel collapse |
| `ParticleBurst` | Compositor-internal particle system | N/A (physics) | 400ms | Boot complete, celebration |
| `ScanlineSweep` | Compositor-internal sweep pattern | Linear | 2000ms | Boot initialization state |

**Easing functions used:**

| Easing name | Math approximation | Feel |
|---|---|---|
| Ease-out | `1 - (1-t)^3` | Fast start, gentle finish |
| Ease-in | `t^3` | Slow start, fast finish |
| Ease-in-out (sine) | `0.5 * (1 - cos(π*t))` | Symmetrical, smooth |
| Spring (ease-out-back) | Overshoot formula with settle | Bouncy, energetic |
| Linear | `t` | Constant speed |

`t` is the normalized animation progress: `elapsed_ms / ceiling_ms`, clamped to [0.0, 1.0].

### LUNA.AI Expression Animations

LUNA.AI's Presence Engine drives animations on the `LUNA_ISLAND` surface through a special animation path. Rather than sending standard `LGP_SEND_MOTION` messages, the Presence Engine sends an **expression animation request** that the animation engine handles with higher priority:

```
TODO:
Decision not yet finalized.
Reason: The protocol between luna-ai-d (Presence Engine) and the LGP compositor
for LUNA Island expression animations has not been specified.
LUNA Island's Live2D model (if enabled) requires a different animation model —
Live2D has its own blend tree and parameter system, not the LGP surface transform model.
This requires a separate Volume III/IV document.
For v1: LUNA Island uses standard LGP surface transforms (opacity, scale, position).
Live2D integration (if it happens) is a v1.5 or v2 feature.
```

For v1, LUNA Island animations use the standard Motion Vocabulary:
- LUNA mode change: `Fade` out current expression state → `Fade` in new state
- LUNA thinking (LLM processing): `Pulse` animation on the Island surface
- Notification arrival: `Slide` in notification card, `Ripple` on Island
- Boot complete: `ParticleBurst` from Island position

### Concurrent Animation Budget

The animation engine tracks a **complexity score** for all active animations. When the score exceeds the complexity budget, lower-priority animations are paused.

Complexity scoring (per active animation):

| Motion Class | Complexity score | Rationale |
|---|---|---|
| `Fade` | 1 | Cheap: only opacity change |
| `Slide` | 2 | Cheap: only translation |
| `Scale` | 3 | Moderate: scale with filtering |
| `Ripple` | 3 | Moderate: alpha + scale from point |
| `Pulse` | 2 | Cheap: looping but simple |
| `Expand` / `Collapse` | 5 | Expensive: multiple property changes |
| `ParticleBurst` | 10 | Expensive: particle system |
| `ScanlineSweep` | 8 | Expensive: compositor-internal pattern |

**Complexity budget by performance profile:**

| Profile | Max complexity score | Notes |
|---|---|---|
| Default | 30 | Standard desktop experience |
| Calm (🌿) | 15 | Reduced motion for battery saving |
| Focus (⚡) | 20 | Slightly reduced — fewer distractions |
| Gaming (🎮) | 40 | Higher budget — gaming context accepts GPU use |
| Unleashed (🚀) | 50 | Maximum concurrent animation |

When the budget is exceeded, animations are paused in priority order (system animations > AI animations > application animations). The lowest-priority animations are deferred first.

```
TODO:
Decision not yet finalized.
Reason: The complexity scores and profile budgets above are initial estimates.
Must be validated on reference hardware under realistic workloads.
The Luna Performance Lab (Architecture_Session_3.md → Volume V) will expose
the animation frame budget as a user-configurable parameter in Experimental Mode.
```

---

## Technical Details

### Per-Frame Cost

The animation engine's per-frame work is O(N) where N is the number of active animations. For a typical desktop with a few window transitions and the LUNA Island idle animation, N is usually under 10. The engine is designed for N < 100 concurrent animations.

At N > 100, the complexity budget kicks in before the engine's O(N) cost becomes significant.

### Particle System (ParticleBurst, ScanlineSweep)

`ParticleBurst` and `ScanlineSweep` are compositor-native animations — they are not surface transforms but compositor-rendered visual effects drawn directly into the framebuffer before the final KMS flip.

The particle system is a minimal implementation:
- Fixed particle pool (64 particles per burst)
- CPU-simulated particle physics: position, velocity, lifetime, opacity
- Particles are rendered as compositor-drawn quads during the GPU render pass
- No external particle library

```
TODO:
Decision not yet finalized.
Reason: Whether particle effects should be GPU-computed (compute shader)
or CPU-simulated has not been decided for v1.
CPU simulation for 64 particles is fast (<< 1ms per frame).
GPU compute shader would allow more particles but adds pipeline complexity.
For v1: CPU simulation. GPU particles are a v1.5 consideration.
```

### Animation State Machine

The animation engine uses a state machine per surface to manage animation lifecycle:

```
IDLE
  │ LGP_SEND_MOTION received
  ▼
PLAYING
  │ elapsed < ceiling_ms
  │ (advancing each frame)
  │
  ├──→ COMPLETED (elapsed ≥ ceiling_ms → snap to final state)
  │
  └──→ INTERRUPTED (new LGP_SEND_MOTION for same surface mid-animation)
            │
            └──→ PLAYING (new animation begins from current transform state)
```

When an animation is **INTERRUPTED** by a new motion command, the new animation starts from the surface's current (mid-animation) transform state, not from the surface's resting state. This prevents visual "pops" when chaining animations quickly.

### Budget Enforcement Log

The animation engine logs budget enforcement events:

| Event | Log level | Message |
|---|---|---|
| Animation started | DEBUG | `[anim] surface=N class=Fade started` |
| Animation completed normally | DEBUG | `[anim] surface=N class=Fade complete (150ms)` |
| Animation auto-completed (ceiling hit) | WARN | `[anim] surface=N class=Slide BUDGET EXCEEDED (220ms > 200ms) — snapped` |
| Complexity budget exceeded, animation deferred | WARN | `[anim] complexity budget exceeded, deferring surface=N class=Expand` |

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Live2D model animation for LUNA Island | v1.5 | Requires separate Live2D renderer integration |
| Spring physics easing system | v1.5 | More natural motion for expand/collapse |
| GPU particle system | v1.5 | More particles for `ParticleBurst` without CPU cost |
| Per-output animation timing | v1.5 | Different refresh rates on different monitors need independent animation clocks |
| Reduced-motion mode | v1.5 | User accessibility option — snaps all animations to final state immediately |
| Audio-synchronized animations | v2 | If DL-P02 (sound design) is resolved — sync animation keyframes to sound events |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Easing function library.** Are the easing functions implemented from scratch in C or borrowed from an upstream math library? For v1, implement from scratch — the formulas are simple (see table above).

2. **LUNA Island expression protocol.** How the Presence Engine communicates expression animation requests to the compositor. Must be specified in Volume IV before LUNA Island implementation.

3. **Live2D integration.** v1.5 or v2 feature. The Live2D SDK license and integration approach must be evaluated — it is an external dependency under Core Law I consideration.

4. **Reduced-motion accessibility mode.** `living_interface_design.md` Open Question 2. The animation engine is the right place to implement it (intercept all `LGP_SEND_MOTION` and snap to final state). Design must be defined before v1.5 begins.

5. **Complexity budget values.** Initial estimates. Must be validated on reference hardware. The Luna Performance Lab will expose them as user controls in Experimental Mode.

6. **Profile-based budget switching.** How the animation engine learns the active performance profile has not been specified. Likely: luna-ai-d (which manages profiles) sends a compositor message to update the complexity budget. Must be a Decision Log entry.

---

## AI Context

An AI agent implementing animated UI elements must understand:

- The animation engine runs inside the compositor. Application code does not call the animation engine directly — it sends `LGP_SEND_MOTION` (via LunaGUI's `widget.animate()`) and the compositor handles the rest.
- The Budget ceiling is a hard limit. Design animations to finish in less than the ceiling, not exactly at the ceiling. A `Slide` animation designed for exactly 200ms will occasionally be cut off at 200ms by frame timing variance.
- `Pulse` is the only looping motion class. All other motion classes are one-shot. If you need a looping animation beyond `Pulse`, you must chain animations (complete → restart) via the `on_complete` callback — do not use a non-Pulse class in a loop.
- Never animate the same surface with two simultaneous animations. The second animation interrupts the first and starts from the mid-animation transform state. If you need multiple properties to animate simultaneously, they should be part of a single motion class (e.g., `Expand` moves both position and scale at once).
- `ParticleBurst` is reserved for significant positive events. Do not use it for routine UI feedback. It is the "boot complete", "task finished" animation — not a button press response.
- `ScanlineSweep` is reserved for the boot sequence and initialization states. Do not use it for in-session UI transitions.
- Animation engine log entries at WARN level indicate a design problem in the animated component. If you see `BUDGET EXCEEDED` for a surface you own, the animation that surface plays needs to be redesigned to finish within budget.

---

*Document: `Volume III / 05_animation_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 02_rendering_pipeline.md, 03_compositor.md, core_laws.md (Law III — Animation Budget, Motion Vocabulary), living_interface_design.md, luna_personality.md*
*Informs: 06_theme_engine.md, Volume IV (LUNA expression system), Volume V (Performance Lab animation controls)*
