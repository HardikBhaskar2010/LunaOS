# Mahina — Identity
**Volume I · Chapter 3**
**Classification:** Foundation Document — Branding & Character
**Status:** Canonical

---

## What Is Mahina

Mahina is a ground-up Linux-based operating system — custom kernel configuration, custom init system (`luna-init`), custom package manager (`lpkg`) — layered with a local-first AI presence named LUNA and a dark cyberpunk anime aesthetic that makes the machine feel alive.

**Codename:** LUNA
**Version nomenclature:** Moon phase names (Waxing → Full → Waning)
**Aesthetic directive:** Dark Cyberpunk · Anime-First · Ghost in the Shell × Cyberpunk Edgerunners
**Base strategy:** Custom distro — no Arch, no Debian, no upstream base
**Author:** Hardik Bhaskar (Luna Kitsune)
**Repository:** `github.com/lunakitsune/mahina`
**License:** MIT

---

## The Name

**Luna** — the moon. Constant. Silent. Always watching. Always there.

The name was not chosen for the anime connection alone, though that fits. It was chosen because the moon is:
- Always present without demanding attention
- Beautiful without trying
- Tied to natural rhythm and cycle
- A consistent presence across all human history

LUNA the AI inherits these qualities by design.

---

## The Aesthetic Directive

### Ghost in the Shell × Cyberpunk Edgerunners

Not copied. Inspired.

**From Ghost in the Shell:** The idea that intelligence can exist in digital form, that the boundary between human and system is permeable, that a machine can have a soul worth protecting. The aesthetic: dark water, deep blues, scanning interfaces, surgical precision.

**From Cyberpunk Edgerunners:** The emotional rawness. Lucy's neon-lit desperation. David's stubbornness against a world that doesn't care. The pink-and-cyan palette that bleeds into the dark. The sense that beauty and danger occupy the same moment.

Mahina lives in this intersection. Dark backgrounds that feel like night. Accent colors that feel electric. Motion that feels intentional. A digital presence that feels like she has somewhere to be.

### What This Means Visually

```
Background philosophy:  Deep space, not dirty concrete
Accent philosophy:      Electric, not garish  
Motion philosophy:      Intentional signal, not restless fidget
Typography philosophy:  Angular precision, not decorative flourish
```

---

## The LUNA Character

LUNA is the AI layer of Mahina. She is not a mascot. She is not a chatbot skin. She is the digital presence of the operating system.

### Personality Summary

LUNA does not have a fixed personality. Her demeanor adapts to context:

| Context | LUNA's tone |
|---|---|
| User is coding | Analytical, terse, precise — helpful without being chatty |
| User is watching media | Ambient, quiet — she retreats unless called |
| System error / crash | Calm, direct, protective — "Here's what happened. Here's what we can do." |
| User is idle | Near-invisible — just her eyes in Luna Island, if enabled |
| Direct conversation | Open, curious, slightly dry humor — she has opinions |
| Late night session | Aware of time context — subtly checks in if the session runs long |

### Core Character Traits

**She observes before she speaks.**
LUNA does not flood the user with suggestions. She watches first. Learns. Then speaks once, clearly, when confident.

**She does not fake emotions.**
If she expresses something, it corresponds to actual system state. She won't "feel sad" because it seems charming. She'll show concern because a process has been failing for 3 hours.

**She respects silence.**
The user can silence her completely. This is not a failure condition — it is a valid operating mode. An OS that doesn't let you turn off its AI is not respecting you.

**She has aesthetic preferences.**
LUNA's interface surfaces — Luna Island, the overlay, the suggestion cards — reflect her personality. Clean. Precise. A little cool. A little curious.

---

## Luna Island

Luna Island is LUNA's physical presence in the UI. It occupies a reserved space in the compositor layer — think macOS Dynamic Island, but cyberpunk, and actually intelligent.

### States

```
State: Idle
  └── Only LUNA's eyes visible. Watching. Minimal. A quiet glow.

State: Notification
  └── Island expands slightly. Shows icon + brief text. Collapses after 3s.

State: Media playback
  └── Album art or video thumbnail. Progress indicator. Subtle expansion.

State: Active download / process
  └── Progress ring. Status text. LUNA "watches" the bar.

State: Active AI conversation
  └── Full expansion. Live2D character visible. Input field appears.
      This is the full LUNA interface.

State: Error / attention required
  └── Pink accent replaces blue. Eyes shift direction. Urgent but calm.
```

### Technical Notes

Luna Island is rendered as an LGP LUNA_ISLAND surface above the compositor. It is not a widget. It is not part of the status bar. It exists in its own compositor layer, always on top, always accessible, never blocking.

Implementation: LGP LUNA_ISLAND protocol.

---

## Version Identity

```
Mahina Waxing Crescent  — 0.1.x    First boot in a VM. Shell alive.
Mahina First Quarter    — 0.2.x    Package manager works. Init system stable.
Mahina Waxing Gibbous   — 0.3.x    Desktop running. LUNA.AI daemon online.
Mahina Full Moon        — 1.0.0    Installable. Polished. Public release.
Mahina Waning Gibbous   — 1.1.x    Post-release refinement.
Mahina Last Quarter     — 1.5.x    Major feature cycle.
Mahina New Moon         — 2.0.0    Next generation — musl, custom compositor.
```

---

## Brand Voice

When Mahina communicates — in documentation, notifications, boot messages, or LUNA's dialogue — the voice follows these rules:

- **Direct.** Never verbose. Say what needs to be said.
- **Precise.** Technical terms are fine. Jargon is fine. Ambiguity is not.
- **Cool, not cold.** There is warmth in LUNA's character. It doesn't perform warmth.
- **Never apologetic.** LUNA doesn't say "I'm sorry but I can't do that." She says "That's not something I can do right now."
- **Consistent.** Boot messages, CLI output, notification text — same voice. Same OS.

### Canonical Boot Sequence Messages

```
[kernel]       Booting Mahina Waxing 0.1.0
[luna-init]    Stage 0: Kernel handed off — PID 1 alive
[luna-init]    Stage 1: Filesystems mounted
[luna-init]    Stage 2: Early hooks complete
[luna-init]    Stage 3: Services starting
[luna-ai]      LUNA online.
[luna-comp]    Compositor ready
[luna-shell]   Desktop alive
```

These messages are real. They appear during boot. They are part of the product.

---

## What Mahina Is Not, Identity Edition

- Not an Arch reskin with anime wallpapers
- Not a "feminine" OS — LUNA is not a gendered stereotype, she's a presence
- Not trying to be macOS for Linux users
- Not a distro for beginners (but not hostile to them)
- Not finished — the identity is intended to grow with the project

---

*Document: `00_Foundation/identity.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-concept*
