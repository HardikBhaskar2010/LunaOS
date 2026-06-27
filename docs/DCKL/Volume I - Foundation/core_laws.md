# Mahina — Core Laws
**Volume I · Chapter 4**
**Classification:** Foundation Document — Constitutional
**Status:** Locked. Changes require full project review.

---

## Preamble

These laws exist above all other documents. They are not goals or aspirations. They are hard constraints. A feature that violates a Core Law does not ship, regardless of how useful it seems, how much time was spent building it, or how many users request it.

Every new contributor, every AI assistant working on this codebase, every future maintainer must read and acknowledge these laws before touching Mahina code.

---

## Law I — Own Every Layer

```
You must understand everything running on Mahina, because you put it there.
```

### What This Means

No component ships in Mahina without a clear answer to:
1. What does this do?
2. Why is this the right choice over alternatives?
3. Where does it touch the rest of the system?
4. Who maintains it, and what's the fallback if it's abandoned?

### Enforcement

- Every upstream dependency is recorded in `docs/dependencies.md` with justification
- Every kernel config option is commented in `kernel/.config.notes`
- No "it probably works" integrations — tested or not shipped
- Mahina never auto-updates without user command

### The Distro Exception

We do not write our own C library. We do not write a kernel scheduler from scratch. The law is about *understanding and ownership*, not reinvention for its own sake.

Using well-understood lower-level abstractions (like DRM/KMS) is fine — we understand exactly what they do and they provide the hardware interface for LGP. Blindly pulling a random dependency because it solved a problem in a Stack Overflow post is not fine.

---

## Law II — Local First, Cloud When Useful

```
Mahina must be fully functional with no internet connection.
Every cloud feature must be explicitly opt-in.
```

### What This Means

| Feature | Offline status |
|---|---|
| LUNA.AI pattern detection | ✅ Fully offline — SQLite, local analysis |
| LUNA.AI local model inference | ✅ Fully offline — Ollama, local weights |
| Package installation from cache | ✅ Offline if previously downloaded |
| Package index update | ❌ Requires internet — but not blocking |
| Cloud bridge (OpenRouter) | ❌ Requires internet — explicit opt-in only |
| Software updates | ❌ Requires internet — never automatic |

### Privacy Sub-Law

All user data is stored in `~/.luna/`. This directory:
- Is never read by any Mahina component except LUNA.AI daemon with user permission
- Is never transmitted anywhere except when user explicitly invokes `luna bridge --send`
- Can be completely wiped with `luna memory --clear` — this command cannot be disabled
- Can be inspected in full with `luna memory --audit` — human-readable log

**LUNA.AI observation is deny-by-default.** New applications are never observed without explicit `~/.luna/config/observe.toml` opt-in.

---

## Law III — Aesthetic Is Functional

```
No animation ships unless it communicates system state.
No color choice is arbitrary.
Every motion has meaning.
```

### The Animation Budget

| Interaction type | Max duration |
|---|---|
| Button press / click response | ≤ 100ms |
| UI element transition | ≤ 200ms |
| Window open / close | ≤ 300ms |
| State change animation | ≤ 400ms |
| Complex narrative motion (boot splash) | ≤ 2000ms |

Animations that exceed budget must be reworked, not shipped with a TODO.

### The Color Semantic Contract

These color meanings are locked. A color used outside its semantic role violates this law:

| Color | Hex | Semantic role |
|---|---|---|
| LUNA Blue | `#00D4FF` | Primary — active, connected, LUNA presence |
| LUNA Purple | `#7B2FBE` | Secondary — depth, intelligence, processing |
| LUNA Pink | `#FF2D78` | Alert — error, warning, urgent attention |
| LUNA Green | `#00FF88` | Success — online, complete, healthy |
| LUNA Amber | `#FFB800` | Caution — degraded, slow, needs attention |

Using LUNA Pink for decorative purposes violates this law. It means something is wrong.

### The Motion Vocabulary (Locked)

```
Glitch flicker     →  Error state. Application crash. System fault.
Scanline sweep     →  Initialization. Loading. Coming online.
Particle burst     →  Task completed successfully.
Data stream scroll →  AI model is generating. Processing in progress.
Fade + bloom       →  Notification arrived. Non-blocking attention.
Ripple expand      →  Input confirmed. Click registered.
Slow pulse         →  LUNA.AI passive observation. Background activity.
Fast pulse         →  LUNA.AI active query. Computing now.
Glitch + freeze    →  Critical error. Requires intervention.
```

If a new motion type is needed, it must be added to this vocabulary with a defined semantic meaning before it is used anywhere in the UI.

---

## Law IV — Silence Before Suggestion

```
LUNA.AI must be correct before it speaks.
A wrong suggestion is worse than no suggestion.
```

This law governs LUNA.AI behavior specifically.

### The Confidence Threshold

LUNA.AI may not surface a suggestion unless:
- The pattern has been observed ≥ 3 times (configurable, never below 2)
- The confidence score exceeds 0.75 (on 0.0–1.0 scale)
- The current context matches the pattern context within acceptable variance
- The suggestion is not in the user's dismissed/suppressed list

### The Three-Strike Rule

If a user dismisses the same suggestion type three times:
1. Strike 1: Reduce pattern weight
2. Strike 2: Reduce further, increase threshold
3. Strike 3: Suppress the suggestion permanently until user manually re-enables

LUNA.AI does not argue with the user. She backs off.

### The Silence Mode

`luna observe --off` kills all observation immediately. Every module stops. No suggestions fire. LUNA.AI daemon remains running (for CLI queries) but is completely passive.

This mode cannot be overridden by any other system component, scheduled task, or update.

---

## Law V — The User Owns the Machine

```
Mahina never performs irreversible actions without explicit confirmation.
The user always has a way out.
```

### What This Covers

- `lpkg remove <core-package>` must warn and require `--force` if removing critical system packages
- No automatic updates of any kind — `lpkg update` is always a manual command
- `luna-init` service management requires explicit commands — services do not restart themselves without configuration
- No telemetry of any kind, ever, under any circumstances
- No analytics, no crash reports sent anywhere, no "improve Mahina" data collection

### The Exit Promise

A user must always be able to:
1. Turn off LUNA.AI completely
2. Wipe all AI memory
3. Disable any observation module individually
4. Boot into a minimal session without any Mahina userland components
5. Remove Mahina cleanly and leave a functioning base Linux system

These capabilities are not features. They are rights.

---

## Law VI — Documentation Is Code

```
Undocumented behavior does not exist.
If it's not documented, it wasn't intentionally designed.
```

### What This Means

- Every `luna` CLI command has a man page entry
- Every LUNA.AI behavior has a corresponding entry in `03_LUNA_AI/`
- Every `lpkg` command is documented in `04_Userland_SDK/lpkg.md`
- Every kernel config decision is noted in `01_Core_Architecture/kernel.md`
- Every Mahina service has a `luna-init` service file and a doc entry

### The AI Readability Standard

All documentation in this project must be usable as AI context. This means:

- Concrete over abstract ("port 7734" not "a local port")
- Structural over narrative where precision matters
- Explicit decisions, not implied ones
- Explicit TODOs rather than silent gaps
- Every major component has an architecture diagram (ASCII is fine)

This standard exists because LUNA.AI, Claude Code, and future AI tools will read these docs to help build Mahina. Bad docs produce bad AI assistance. Good docs produce good AI assistance.

---

## Amendments

These laws can be amended. The process:

1. Open a `law-amendment` issue in the repository
2. Clearly state which law is being amended and why
3. Provide a concrete example of a feature or decision that the current law incorrectly blocks
4. Wait for a minimum 72-hour review period
5. Document the decision in `00_Foundation/decision_log.md`

Laws I, II, and V are the hardest to amend. They represent the soul of the project.

---

*Document: `00_Foundation/core_laws.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-concept*
*This document is versioned. Breaking changes to these laws are major version events.*
