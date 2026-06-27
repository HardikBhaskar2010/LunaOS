# Mahina — Architecture Rules
**Volume VI · Chapter 03**
**Classification:** Development Bible — Design Standards
**Status:** Canonical

---

## Purpose

This document outlines the hard rules for system architecture. While the Core Laws (`Volume I / 04_core_laws.md`) define the philosophical goals of Mahina, these rules define the engineering mechanics.

---

## The Rules

### 1. The Single Owner Rule
Every capability, file, or resource in Mahina has exactly one process that owns it.
- **Rule:** Do not write to a file or socket owned by another process.
- **Enforcement:** `Volume II / 13_component_ownership.md` is the canonical reference.

### 2. The Fallback Rule
Every component must gracefully handle the failure of its dependencies.
- **Rule:** If the compositor dies, `luna-shell` must wait and reconnect when it restarts. If the LLM goes out of memory, `luna-ai-d` must enter DEGRADED mode and rely on the Presence Engine.
- **Enforcement:** Components must implement connection retry logic and degraded state handling.

### 3. The Isolation Rule
Processes must be strictly isolated.
- **Rule:** Never share memory directly between applications. All shared memory must be brokered by the compositor via LGP (for surfaces and clipboard).
- **Enforcement:** Third-party applications run under strict AppArmor profiles.

### 4. The IPC Rule
Use the right IPC for the right job.
- **Rule:** 
  - Use **LGP** for anything graphical (surfaces, clipboard, input, semantic colors).
  - Use **D-Bus** for system state, notifications, hardware events, and LLM queries.
  - Use **Unix Domain Sockets** for direct daemon-to-daemon streams if D-Bus overhead is too high.
- **Enforcement:** Documented in `Volume II / 07_ipc.md`.

### 5. The Initialization Rule
The boot sequence must be predictable.
- **Rule:** `luna-init` is the sole arbiter of boot order. Do not create background daemons that attempt to fork and daemonize themselves.
- **Enforcement:** All system services must run in the foreground (supervised by `luna-init`) and log to `stdout`/`stderr`.

---

*Document: `Volume VI / 03_architecture_rules.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Status: Active*
