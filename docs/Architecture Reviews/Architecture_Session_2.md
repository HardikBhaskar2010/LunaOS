# Architecture Review Meeting #2 — Decisions

## DL-004R — Graphics Architecture (Supersedes DL-004)

**Status:** Approved

### Decision

Mahina will adopt a **hybrid graphics architecture**.

Applications will normally communicate through the LunaGUI toolkit while advanced applications may communicate directly with the Luna Graphics Protocol (LGP) where appropriate.

This preserves simplicity for application developers while allowing high-performance software to utilize the graphics protocol directly.

---

## DL-005 — Root Filesystem

**Status:** Provisional

### Decision Goals

The root filesystem must prioritize:

* Maximum performance
* Simple recovery

Automatic snapshots will be created before:

* System updates
* Kernel updates

Manual snapshots remain available.

Filesystem implementation (Ext4 vs. Btrfs) remains under evaluation.

---

## DL-006 — EFI Layout

**Status:** Approved

Mahina will initially follow the standard Linux UEFI partition layout.

This preserves compatibility with existing firmware, installers, dual-boot environments, and recovery tooling.

Future internal directory structures may differ, but the EFI partition remains standards-compliant.

---

## DL-007 — Wireless Backend

**Status:** Provisional

Current priority:

* Maximum hardware compatibility
* Strong performance

Backend implementation remains under evaluation.

The chosen implementation should provide broad device support while minimizing latency and maintenance burden.

---

## DL-008 — DNS Strategy

**Status:** Approved

Version 1.x will use the existing Linux DNS resolver.

A future LunaDNS service may replace it after sufficient architectural research.

The existing resolver provides stability while allowing future innovation.

---

## DL-009 — Time Synchronization

**Status:** Approved

Mahina will initially rely on the existing Linux time synchronization service.

Time synchronization is not considered a differentiating subsystem for Version 1 and therefore should prioritize reliability over reinvention.

---

## DL-010 — Package Privilege Escalation

**Status:** Approved

Package installation requiring elevated privileges may request authorization through the LUNA graphical permission interface.

Terminal authentication remains available.

Graphical authorization becomes the preferred user experience.

---

## DL-011 — Package Installation Scope

**Status:** Approved

Packages should install per-user by default.

System-wide installation remains available when explicitly requested by an administrator.

---

## DL-012 — Transaction Rollback

**Status:** Approved

Every package transaction should be atomic whenever possible.

If installation fails, the package manager should restore the previous system state automatically.

Reliability takes precedence over partial installation.

---

## DL-013 — Repository Policy

**Status:** Approved

Mahina will support:

* Official repositories
* Community repositories
* Third-party repositories

Creativity should not be artificially restricted.

Instead of blocking software sources, Mahina will provide:

* Signature verification
* Reputation indicators
* Malware scanning
* Security analysis
* User warnings

Security should be achieved through verification rather than limitation.

---

## DL-014 — Third-Party Isolation

**Status:** Approved

Third-party applications should execute inside an isolated sandbox by default.

Users may relax restrictions explicitly.

Security defaults should favor containment without preventing advanced workflows.

---

## DL-015 — AI Runtime

**Status:** Approved

LUNA consists of two independent systems.

### LUNA Presence Engine

Starts automatically during system boot.

Responsibilities:

* Luna Island
* Context awareness
* Expressions
* Notifications
* Lightweight behavior

### LLM Inference Engine

Loads lazily.

The language model initializes only when:

* The user starts a conversation.
* Voice interaction begins.
* AI automation is requested.
* Explicit reasoning is required.

This minimizes idle memory consumption while preserving responsiveness.

---

## DL-016 — Context Service

**Status:** Approved

A lightweight background context service runs after boot.

During operating system installation the user explicitly grants or denies permissions.

Only approved data sources may be observed.

No hidden monitoring is permitted.

---

## DL-017 — Persistent Memory

**Status:** Approved

LUNA maintains memory across reboots.

During shutdown a protected summarization process produces a condensed memory record.

This memory is encrypted and stored in a dedicated protected location.

Long-term memory must remain entirely under user control.

---

## DL-018 — Mahina Success Criteria

**Status:** Canonical

Version 1.0 succeeds when:

* The operating system feels technically impressive.
* The operating system genuinely feels alive.

Neither goal may come at the expense of the other.

Performance and Presence are equal pillars of Mahina.
