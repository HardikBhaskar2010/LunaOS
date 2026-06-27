# Architecture Review Session 4

## Project: Mahina

## Topic: Graphics, System Foundations & Stage 2 Architecture Decisions

---

# Session Information

**Session ID:** AR-004

**Status:** Accepted

**Project Phase:** Stage 2 Architecture Finalization

**Purpose**

This Architecture Review resolves the remaining high-priority architectural questions blocking Stage 2 implementation of Mahina. Decisions recorded here supersede recommendations made in previous documentation where applicable.

All accepted decisions shall be converted into Decision Log entries (DL-025 onward) and become part of the Mahina canonical architecture.

---

# Decision 1 — LGP Wire Format

## Decision

**Accepted Option:** A — TLV (Type-Length-Value)

## Reasoning

LGP should remain dependency-free, easy to debug, and simple to evolve.

A TLV binary protocol provides:

* Easy versioning
* Simple C implementation
* Small parser
* Excellent debugging using hex dumps
* Append-compatible message evolution

No external serialization framework shall be required.

---

# Decision 2 — GPU Backend Strategy

## Decision

Hybrid development strategy.

Stage 2

* Software renderer

Purpose:

Prove the compositor, rendering pipeline and protocol before GPU complexity is introduced.

Stage 3

* Vulkan becomes the primary renderer.

Fallback

* OpenGL/EGL fallback only when Vulkan is unavailable on supported hardware.

The compositor itself communicates only with the `lgp-render` abstraction layer.

GPU implementation details remain isolated.

---

# Decision 3 — Root Filesystem

## Decision

Root filesystem:

**Btrfs**

Reasons:

* Native snapshots
* Rollback capability
* Future recovery system
* Copy-on-write
* Better long-term architecture for Mahina

Automatic snapshots shall occur before:

* System updates
* Kernel updates
* Driver updates

---

# Decision 4 — Default System Typeface

## Decision

**Bitcount**

Bitcount becomes the canonical Mahina system font.

Usage policy:

* Boot screen
* Login
* Dock
* Luna Island
* LUNA responses
* Branding
* Major UI headings

For dense reading, documentation, terminals, editors, and productivity applications, Mahina may use condensed or optimized companion fonts selected for readability.

The design goal is:

> Personality where it matters.
> Readability where it matters.

---

# Decision 5 — Text Rendering

## Decision

FreeType + HarfBuzz

Reasons:

* Industry standard
* Mature
* Excellent Unicode support
* High quality shaping
* No need to reinvent typography

No custom text shaping engine will be developed for Version 1.

---

# Decision 6 — Layout Engine

## Decision

Version 1

Flexbox-style layout model.

Future

Constraint-based layouts may be introduced in Version 1.5 or later.

Reasoning

The majority of Mahina applications can be implemented using a flexible box layout.

This minimizes implementation complexity while leaving room for future expansion.

---

# Decision 7 — Compositor Readiness Signal

## Decision

D-Bus readiness signal.

The compositor shall publish a readiness event through D-Bus once graphical services become available.

Dependent services may wait on this signal instead of polling files.

---

# Decision 8 — Input Backend

## Decision

libinput

Reasoning

Mahina is not intended to reimplement years of touchpad and input-device compatibility work.

libinput provides:

* Touchpads
* Gestures
* Device quirks
* Pointer acceleration
* Tablet support

while allowing Mahina to focus engineering effort elsewhere.

---

# Decision 9 — Clipboard Architecture

## Decision

Clipboard becomes an LGP protocol extension.

The compositor owns clipboard state.

Clipboard access remains governed by the Permission Engine.

Applications never communicate directly with one another.

---

# Decision 10 — Luna Island Interaction

## Decision

Hybrid interaction model.

Short Click

Expand Luna Island into a compact interaction panel.

Long Press

Expand into the complete conversational interface.

This preserves Luna's presence while keeping interactions fluid.

---

# Decision 11 — Screen Lock Ownership

## Decision

Dedicated process:

`luna-lock`

Reasons

* Better privilege separation
* Cleaner architecture
* Independent lifecycle
* Simpler recovery
* Better security boundaries

The lock screen is not owned by luna-init nor by luna-shell.

---

# Decision 12 — Wireless Backend

## Decision

Version 1

wpa_supplicant

Reason

Maximum compatibility remains the highest networking priority.

Migration toward iwd may be reconsidered after broader hardware validation.

---

# Decision 13 — Theme Change Notifications

## Decision

Both mechanisms.

* LGP notification
* D-Bus signal

Graphical applications update through LGP.

Non-graphical services receive D-Bus notifications.

---

# Decision 14 — Swap Strategy

## Decision

Swapfile

Combined with

zram

Swap hierarchy:

1. RAM
2. zram
3. Swapfile

This provides good responsiveness while maintaining flexibility for future installer changes.

---

# Decision 15 — Built-in Themes

## Decision

Version 1 ships only:

**Luna Dark**

Reason

Dark mode represents Mahina's visual identity.

LUNA's expressions, Island behavior, motion language, and semantic colors are designed around dark environments.

Light mode is intentionally postponed until a future release.

---

# Decision 16 — Accessibility

## Decision

AT-SPI2

Reason

Mahina should support existing accessibility tooling rather than introducing unnecessary incompatibility.

Accessibility is considered a first-class requirement.

---

# Decision 17 — Voice

## Decision

Local TTS ships as an optional component.

Default state:

Disabled.

Users explicitly enable voice functionality.

No mandatory online voice services.

---

# Decision 18 — AI Runtime Memory

## Decision

The AI runtime consists of two independent layers.

### Presence Engine

Starts automatically during every boot.

Responsibilities include:

* Luna Island
* Eyes
* Expressions
* Context monitoring
* Lightweight behavior

This component is intentionally lightweight.

### LLM Runtime

Loads lazily.

Initialization occurs only when:

* Conversation begins
* Voice interaction starts
* AI reasoning is requested
* User explicitly loads a model

Memory allocation scales according to:

* Selected model
* Available RAM
* User configuration

No fixed memory reservation exists.

---

# Decision 19 — Boot Splash Transition

## Decision

Version 1 accepts a brief transition between the boot splash and compositor.

Reason

This dramatically simplifies early boot architecture.

Visual polish can be improved in future releases without affecting system architecture.

---

# Decision 20 — Conversation Panel Ownership

## Decision

The conversation panel is owned by:

`luna-island`

The panel represents an expanded state of Luna's presence rather than a separate application.

No additional process is introduced.

---

# New Architectural Principle — Typography Philosophy

Typography exists to communicate personality without sacrificing readability.

Distinctive branding fonts should be used where identity matters.

Optimized reading fonts should be used where productivity matters.

Personality should never reduce usability.

---

# New Architectural Principle — AI Runtime Philosophy

The Presence Engine is always alive.

The Intelligence Engine wakes only when needed.

Presence is continuous.

Reasoning is on demand.

---

# Session Summary

Architecture Review Session 4 resolves the remaining Stage 2 blockers and finalizes foundational implementation decisions for:

* Graphics protocol
* Rendering strategy
* Filesystem
* Typography
* Layout engine
* Input backend
* Clipboard architecture
* Accessibility
* Wireless networking
* Swap configuration
* AI runtime
* Boot transition
* Luna Island interaction

These decisions become canonical after corresponding Decision Log entries are created.

---

# Session Status

**Status:** Accepted

**Architecture Confidence:** High

**Implementation Readiness:** Approved for Stage 2

**Next Step:**

Decision Log entries DL-025 through DL-044 recorded in `decision_log.md`. Affected Volume II and Volume III documents updated to reference accepted decisions.
