# Mahina — Decision Status Standard
**Volume I · Chapter 10**
**Classification:** Foundation Document — Process Standard
**Status:** Canonical · All decision log entries must comply with this standard going forward

---

## Purpose

This document defines the **Decision Status system**: a required status field on every architectural decision in Mahina. Without statuses, a Decision Log becomes a graveyard of old decisions with no way to know which ones are still in force, which were superseded, and which are being reconsidered.

This standard applies to:
- All entries in `decision_log.md`
- All "Current Decisions" tables in every Volume I–VII document
- All future Architecture Review Session outcomes

---

## Overview

Every decision has a lifecycle. A decision starts as a **Draft**, gets reviewed and becomes **Accepted**, may later be **Superseded** by a better decision, or **Deprecated** when the thing it governed no longer exists. Some decisions are deliberately **Experimental** — adopted with full awareness that they may be reversed.

Without statuses, you cannot answer the question: *"Is this decision still the law?"*

---

## Status Definitions

| Status | Symbol | Meaning |
|---|---|---|
| `Draft` | 🔵 | Under active discussion. Not yet binding. Implementations based on Draft decisions may need to change. |
| `Accepted` | ✅ | Formally adopted. This is the current law. All implementations must comply. |
| `Experimental` | 🧪 | Adopted for evaluation. Intentionally provisional. Expected to either become Accepted or be replaced after testing. |
| `Deprecated` | 🟡 | Still technically in force but known to be replaced in a future version. Do not build new systems against Deprecated decisions. |
| `Superseded` | ❌ | Replaced by a newer decision (referenced in the Superseded By field). No longer in force. Kept for historical record only. |
| `Revoked` | 🔴 | Withdrawn without replacement. The decision was wrong or circumstances changed. The governed behavior is undefined until a new decision is made. |

---

## Status Transition Rules

```
Status transition state machine:

  [proposed]
       │
       ▼
    DRAFT ──────────────────────────────→ REVOKED (withdrawn before acceptance)
       │
       │ reviewed, no objections, Principal Engineer approves
       ▼
  ACCEPTED ──────────────────────────→ REVOKED (exceptional withdrawal)
       │
       ├──→ EXPERIMENTAL (if accepted for trial, not permanent)
       │         │
       │         ├──→ ACCEPTED (trial successful)
       │         └──→ REVOKED (trial failed)
       │
       ├──→ DEPRECATED (still valid, but planned for replacement)
       │         │
       │         └──→ SUPERSEDED (replacement accepted)
       │
       └──→ SUPERSEDED (directly replaced by newer decision)
                 │
                 └──→ [historical record only — no further transitions]
```

**Rules:**
1. Only the Principal Engineer (Hardik Bhaskar) may change a decision status.
2. `Accepted` → `Superseded` requires the superseding decision to be `Accepted` simultaneously.
3. `Accepted` → `Revoked` is exceptional — must include a written rationale.
4. `Draft` decisions are non-binding. Do not implement systems that only have Draft decisions as their foundation.
5. `Superseded` and `Revoked` entries are never deleted. They remain in the Decision Log for historical reference.

---

## Decision Record Format

Every decision in `decision_log.md` must use this format going forward:

```markdown
### DL-XXX — [Decision Title]
**Status:** [Status Symbol + Status Name]
**Date:** YYYY-MM-DD
**Supersedes:** DL-YYY (if applicable)
**Superseded By:** DL-ZZZ (if applicable — filled in when superseded)

**Decision:**
[One paragraph stating what was decided. Plain language. No ambiguity.]

**Rationale:**
[Why this decision was made. What alternatives were considered and rejected.]

**Consequences:**
[What changes as a result of this decision.]

**Affected Documents:**
[List of DCKL documents that must be updated to reflect this decision.]
```

---

## Existing Decision Log — Status Assignments

The following is the canonical status assignment for all existing Decision Log entries. Apply these retroactively.

| DL # | Title | Status |
|---|---|---|
| DL-001 | No Upstream Distro Base | ✅ Accepted |
| DL-002 | Init System: luna-init in C | ✅ Accepted |
| DL-003 | Package Manager: lpkg in Python (v1) | ✅ Accepted |
| DL-004 | Compositor: Hyprland (v1) → wlroots custom (v2) | ❌ Superseded by DL-004R |
| DL-004R | Graphics Architecture — Hybrid Model | ✅ Accepted |
| DL-005 | Bootloader: limine (v1) | ✅ Accepted |
| DL-006 | AI Runtime: Ollama | ✅ Accepted |
| DL-007 | C Library: glibc → musl (planned) | ✅ Accepted |
| DL-008 | Config Format: TOML | ✅ Accepted |
| DL-009 | Kernel Version: Linux 6.6.x LTS | ✅ Accepted |
| DL-010 | LUNA.AI Port: 7734 | ✅ Accepted |
| DL-011 | Root Filesystem — Snapshot-Capable Strategy | ❌ Superseded by DL-027 |
| DL-012 | EFI Partition Layout | ✅ Accepted |
| DL-013 | Wireless Backend | ❌ Superseded by DL-036 |
| DL-014 | DNS Strategy | ✅ Accepted |
| DL-015 | Time Synchronization | ✅ Accepted |
| DL-016 | Package Privilege Escalation | ✅ Accepted |
| DL-017 | Package Installation Scope | ✅ Accepted |
| DL-018 | Package Transaction Rollback | ✅ Accepted |
| DL-019 | Repository Policy | ✅ Accepted |
| DL-020 | Third-Party Application Isolation | ✅ Accepted |
| DL-021 | AI Runtime Architecture — Two Independent Systems | ✅ Accepted |
| DL-022 | Context Service | ✅ Accepted |
| DL-023 | Persistent Memory | ✅ Accepted |
| DL-024 | Mahina Success Criteria | ✅ Accepted |
| DL-025 | LGP Wire Format — TLV Binary | ✅ Accepted |
| DL-026 | GPU Backend Strategy — Staged Implementation | ✅ Accepted |
| DL-027 | Root Filesystem — Btrfs | ✅ Accepted |
| DL-028 | Default System Typeface — Bitcount | ✅ Accepted |
| DL-029 | Text Rendering Library — FreeType + HarfBuzz | ✅ Accepted |
| DL-030 | LunaGUI Layout Engine — Flexbox Model v1 | ✅ Accepted |
| DL-031 | Compositor Readiness Signal — D-Bus | ✅ Accepted |
| DL-032 | Input Backend — libinput | ✅ Accepted |
| DL-033 | Clipboard Architecture — LGP Extension | ✅ Accepted |
| DL-034 | Luna Island Interaction — Hybrid Model | ✅ Accepted |
| DL-035 | Screen Lock Ownership — luna-lock | ✅ Accepted |
| DL-036 | Wireless Backend — wpa_supplicant v1 | ✅ Accepted |
| DL-037 | Theme Change Notifications — Dual Channel | ✅ Accepted |
| DL-038 | Swap Strategy — Swapfile + zram Hierarchy | ✅ Accepted |
| DL-039 | Built-in Themes v1 — Luna Dark Only | ✅ Accepted |
| DL-040 | Accessibility — AT-SPI2 | ✅ Accepted |
| DL-041 | Voice / TTS — Optional Local, Disabled by Default | ✅ Accepted |
| DL-042 | AI Runtime Memory — Dynamic, No Fixed Reservation | ✅ Accepted |
| DL-043 | Boot Splash Transition — Accept the Brief Cut | ✅ Accepted |
| DL-044 | Conversation Panel Ownership — luna-island | ✅ Accepted |

**Pending Decisions (not yet in Decision Log):**

| Pending | Description | Current Status |
|---|---|---|
| DL-P01 | LGP wire format (TLV binary) | ❌ Superseded by DL-025 |
| DL-P02 | GPU backend (Vulkan primary + EGL fallback) | ❌ Superseded by DL-026 |
| DL-P03 | Software renderer for Stage 2 | ❌ Superseded by DL-026 |
| DL-P04 | Root filesystem: ext4 vs. Btrfs final | ❌ Superseded by DL-027 |
| DL-P05 | Default typeface selection | ❌ Superseded by DL-028 |
| DL-P06 | Text rendering library (FreeType + HarfBuzz) | ❌ Superseded by DL-029 |
| DL-P07 | Layout engine: box model v1 | ❌ Superseded by DL-030 |
| DL-P08 | Clipboard protocol | ❌ Superseded by DL-033 |
| DL-P09 | Compositor readiness signal (D-Bus) | ❌ Superseded by DL-031 |
| DL-P10 | Input backend (libinput recommended) | ❌ Superseded by DL-032 |
| DL-P11 | Screen lock ownership | ❌ Superseded by DL-035 |
| DL-P12 | Accessibility bridge (v1.5 AT-SPI2) | ❌ Superseded by DL-040 |
| DL-P13 | LUNA Island click behavior | ❌ Superseded by DL-034 |
| DL-P14 | LUNA voice (TTS in v1?) | ❌ Superseded by DL-041 |
| DL-P15 | Light mode built-in theme | ❌ Superseded by DL-039 |

---

## Decision Review Cadence

Architecture Review Sessions (Discussion_Session_N.md files) are the mechanism by which Draft decisions become Accepted. After each Architecture Review Session:

1. All decisions made in the session are added to `decision_log.md` with status `Accepted`
2. Any decisions superseded by the session are marked `Superseded`
3. Any decisions created during the session as provisional are marked `Experimental`
4. The session document is marked `Status: Closed` with a date

---

## How to Read a Document's "Current Decisions" Table

Every Volume II–VII document includes a "Current Decisions" table at the bottom. These tables should now include a Status column:

```markdown
| Decision | Source | Status |
|---|---|---|
| LGP is the graphics protocol — no Wayland | non_negotiables.md | ✅ Accepted |
| Hybrid architecture: LunaGUI + direct LGP | DL-004R | ✅ Accepted |
| LGP wire format: TLV binary | DL-P01 | 🔵 Draft |
| GPU backend: Vulkan + EGL fallback | DL-P02 | 🔵 Draft |
```

Documents written before this standard was adopted have "Current Decisions" tables without Status columns. They should be updated progressively as each document is revised. Use this document's Existing Decision Log table as the authoritative status source.

---

## AI Context

- Before implementing anything, check the decision status. A `Draft` decision is not binding. Do not implement a system whose foundation is only a Draft decision — it may change.
- `Accepted` decisions are binding immediately. All code must comply.
- `Experimental` decisions are provisionally binding — implement them, but write the code so it can be changed without a rewrite.
- `Superseded` entries in decision_log.md are historical only. Never implement a Superseded decision.
- If you discover a conflict between two `Accepted` decisions, stop and flag it immediately. Do not resolve it by choosing one — it requires human review.
- The Pending Decisions list (DL-P01 through DL-P15) are the open blockers for Stage 2 implementation. These need to be resolved before writing compositor or graphics code.

---

*Document: `Volume I / 10_decision_status_standard.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Addresses: Yellow blocker — "Architecture Decision Status" (Draft/Accepted/Deprecated/Superseded/Experimental)*
*Classification: Canonical process standard*
*Applies to: decision_log.md and all Current Decisions tables in the DCKL*
