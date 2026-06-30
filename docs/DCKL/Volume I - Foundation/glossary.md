# Mahina — Glossary
**Volume I · Chapter 8**
**Classification:** Foundation Document — Reference
**Status:** Canonical · Append-only as new terms are introduced

---

## Purpose

This document is the single source of truth for terminology used across the DCKL. Every term defined here must be used identically in every other document. If a future document introduces a new project-specific term, an entry must be added here in the same change.

---

## Overview

Terms are grouped by category, then alphabetical within each category. Each entry gives a one-line definition and its canonical source document. Where a term has no canonical technical specification yet, this is stated explicitly rather than inferred.

---

## Design Philosophy

A glossary has no design philosophy of its own beyond Core Law VI — *Documentation Is Code*. If a term is used in code, a CLI command, or a doc, and isn't defined here, it does not yet exist as a stable part of the project vocabulary.

---

## Architecture

Not applicable in the structural sense. This document's "architecture" is its maintenance rule:

```
New term introduced in any document
        │
        ▼
Add one-line entry here, same change
        │
        ▼
Reuse identical wording everywhere thereafter
```

---

## Current Decisions

### Project & Identity

| Term | Definition | Source |
|---|---|---|
| **Mahina** | The ground-up Linux-based OS this project builds. Not a distro reskin. | `identity.md` |
| **LUNA** | The OS's local-first AI presence. Not an assistant, not a mascot — the OS's digital presence. | `identity.md`, `vision.md` |
| **DCKL** | "The Divine Collection of Knowledge about Luna" — the name of this documentation project. | System prompt / project structure |
| **Luna Kitsune** | Project author identity, Hardik Bhaskar. | All Foundation docs |

### LUNA-Specific

| Term | Definition | Source |
|---|---|---|
| **Luna Island** | LUNA's physical UI presence; an LGP LUNA_ISLAND surface, distinct from luna-bar and luna-notif. | `identity.md`, `luna_personality.md` |
| **LUNA.AI / luna-ai-d** | The AI daemon. Listens on `localhost:7734`, internal only. | `decision_log.md` DL-010, `philosophy.md` |
| **Personality Modes** | The six defined LUNA behavior states: DEVSHELL, FOCUS, MEDIA, AMBIENT, CONVERSATION, CRISIS. | `luna_personality.md` |
| **Expression Layer** | The seven-priority hierarchy (gaze → color → animation → island state → prop → text → voice) governing how LUNA communicates. | `luna_personality.md` |
| **Cloud bridge** | Optional, explicitly opt-in connection to OpenRouter for cases where local model confidence is insufficient. | `philosophy.md`, `luna_personality.md` |
| **Silence Mode** | Triggered by `luna observe --off`. Kills all LUNA.AI observation; cannot be overridden by any other component. | `core_laws.md` Law IV |
| **Three-Strike Rule** | If a suggestion type is dismissed three times, it is permanently suppressed until manually re-enabled. | `core_laws.md` Law IV |

### System Components

| Term | Definition | Source |
|---|---|---|
| **luna-init** | Custom PID 1, written in C, TOML-based service files. | `decision_log.md` DL-002 |
| **lpkg** | Custom package manager. Python for v1, planned Rust rewrite for v2. | `decision_log.md` DL-003 |
| **LGP (Luna Graphics Protocol)** | Named in project structure (Volume III). **No technical specification exists yet.** | Project structure only — TODO |
| **~/.luna/** | The directory holding all user-specific LUNA data (patterns, memory, preferences). Governed by the Law II privacy sub-law. | `core_laws.md` Law II |

### Visual / Aesthetic

| Term | Definition | Source |
|---|---|---|
| **Living Interface** | The design discipline requiring every UI surface (not only LUNA) to express only real system state through locked color/motion rules. | `living_interface_design.md` |
| **Color Semantic Contract** | The five locked colors (Blue, Purple, Pink, Green, Amber) and their fixed meanings. | `core_laws.md` Law III |
| **Motion Vocabulary** | The nine locked motion types (glitch flicker, scanline sweep, etc.) and their fixed meanings. | `core_laws.md` Law III |
| **Animation Budget** | The locked duration ceilings per interaction class (click ≤100ms, etc.). | `core_laws.md` Law III |
| **NEON/DARK Protocol** | The name given in `philosophy.md` to Mahina's visual language. **No technical specification exists yet** — distinct from, and possibly the precursor naming for, Volume III's "Visual Language" document. | `philosophy.md` — TODO, see Open Questions |

### Process & Records

| Term | Definition | Source |
|---|---|---|
| **DL-XXX** | Decision Log entry notation, e.g. `DL-001`. Append-only; superseded entries are marked, never deleted. | `decision_log.md` |
| **Moon phase versioning** | Mahina version names follow lunar phases (Waxing Crescent → Full Moon → Waning → New Moon). | `identity.md`, `vision.md` |
| **Core Laws** | The six constitutional constraints in `core_laws.md` (Own Every Layer, Local First, Aesthetic Is Functional, Silence Before Suggestion, User Owns the Machine, Documentation Is Code). | `core_laws.md` — see Open Questions for naming discrepancy |

---

## Technical Details

Not applicable — this is a terminology reference, not a technical specification document.

---

## Future Improvements

- As Volume II–VI documents are written, every newly introduced term (e.g., specific lpkg commands, luna-init service file fields, LGP primitives) must be appended to this glossary in the same batch.
- Consider splitting this glossary by volume once term count grows large enough to make a single flat table unwieldy.

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **"Three Laws" vs. "Core Laws" naming discrepancy.** `philosophy.md` titles its section "The Three Laws of Mahina" and enumerates exactly three (Own Every Layer, Local First — Cloud When Useful, Aesthetic Is Functional). `core_laws.md` is titled "Core Laws" and enumerates **six** laws using the same Law I–III names plus three additional ones (Silence Before Suggestion, The User Owns the Machine, Documentation Is Code). This document does not resolve the discrepancy — per project rules, canonical documents are not rewritten here. Flagging for human resolution: is `core_laws.md` an authorized expansion of `philosophy.md`'s three laws (in which case `philosophy.md` should eventually be marked superseded-in-part), or are these two distinct laws documents serving different purposes? Until resolved, any AI agent reading both documents should treat `core_laws.md`'s six laws as the operative, enforceable set, since it is the more recent and more detailed of the two — but should not silently treat `philosophy.md` as outdated without human confirmation.
2. **NEON/DARK Protocol vs. Visual Language (Volume III).** `philosophy.md` names a "NEON/DARK Protocol" as the visual language. Volume III's planned table of contents lists a separate "Visual Language" document. Whether these are the same artifact under two names, or two different things, is undecided.
3. **LGP naming only.** "Luna Graphics Protocol" appears only as a document title in the project structure. No document has yet defined what it protocols *between* (application ↔ compositor? compositor ↔ GPU? LUNA ↔ compositor?).

---

## AI Context

An AI agent should treat every term in the Current Decisions tables above as fixed vocabulary — do not paraphrase, rename, or invent synonyms (e.g., do not call Luna Island a "dock," do not call `luna-init` "the init daemon"). When a document under construction needs a term not listed here, stop and add an entry rather than inventing ad hoc phrasing. Pay particular attention to Open Question 1 — code or documentation that cites "the Three Laws" should be treated as referring to the same conceptual lineage as the Core Laws, but the numerical mismatch (three vs. six) should not be silently flattened.

---

*Document: `00_Foundation/glossary.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-concept*
