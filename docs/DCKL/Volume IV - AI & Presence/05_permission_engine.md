# LunaOS — Permission Engine
**Volume IV · Chapter 5**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · Governs every capability LUNA requests from the user

---

## Purpose

This document specifies the **Permission Engine** — the system that governs what LUNA is allowed to observe, remember, and do on the user's behalf. It is the enforcer of Core Law II (Privacy First) and Core Law V (User Owns the Machine).

The Permission Engine answers one question: **"Is LUNA authorized to do this?"**

Without this document, there is no defined boundary between LUNA's capabilities and LUNA's overreach. The Permission Engine draws that boundary precisely, enforces it at runtime, and makes it visible and controllable by the user.

---

## Overview

```
Permission Engine — enforcement points:

  ┌──────────────────────────────────────────────────────────┐
  │  What LUNA wants to do (proposed action)                  │
  │                                                            │
  │  "Observe the active file path"                           │
  │  "Read clipboard contents"                                │
  │  "Execute a shell command"                                │
  │  "Send a message to a service"                            │
  │  "Access the network"                                     │
  └─────────────────────────────┬────────────────────────────┘
                                │
  ┌─────────────────────────────▼────────────────────────────┐
  │                  PERMISSION ENGINE                        │
  │                                                            │
  │  1. Check permission category                             │
  │  2. Check granted permissions (observe.toml + DB)         │
  │  3. If granted: allow                                     │
  │  4. If not granted: prompt user OR silently deny           │
  │  5. Record the decision                                   │
  └─────────────────────────────┬────────────────────────────┘
                                │
  ┌─────────────────────────────▼────────────────────────────┐
  │  Outcome                                                   │
  │   ALLOWED     → action proceeds                           │
  │   DENIED      → action blocked, reason logged             │
  │   PENDING     → user prompt shown, action waits           │
  └──────────────────────────────────────────────────────────┘
```

---

## Permission Categories

Every action LUNA might take belongs to one of these categories:

| Category | Description | Default | Grant Mechanism |
|---|---|---|---|
| `OBSERVE_APP` | Watch which application is focused | ✅ Granted | observe.toml per app |
| `OBSERVE_FILE` | Watch which file is actively edited | ❌ Denied | Per-app opt-in in observe.toml |
| `OBSERVE_URL` | Watch browser active URL | ❌ Denied | Per-browser opt-in |
| `READ_CLIPBOARD` | Read clipboard contents | ❌ Denied | Explicit user prompt |
| `WRITE_CLIPBOARD` | Write to clipboard | ❌ Denied | Explicit user prompt |
| `EXECUTE_SHELL` | Run a shell command on user's behalf | ❌ Denied | Explicit user prompt per command |
| `READ_FILE` | Read a specific file | ❌ Denied | Per-file, explicit user prompt |
| `WRITE_FILE` | Write or modify a file | ❌ Denied | Per-file, explicit user prompt |
| `ACCESS_NETWORK` | Make an outbound network request | ❌ Denied | Per-host, explicit user prompt |
| `SEND_NOTIFICATION` | Display a notification | ✅ Granted | Granted at install time |
| `OBSERVE_PROCESS` | Watch process list / system stats | ✅ Granted | Granted by default (non-sensitive) |

**Default is deny.** LUNA begins with only the minimum set of permissions granted. Every additional capability requires the user to grant it explicitly — either at install time (via observe.toml) or at runtime (via the LUNA permission dialog).

---

## Permission Storage

Permissions are stored in two places:

### 1. observe.toml — Static Install-Time Permissions

```toml
# ~/.luna/config/observe.toml
# Written by lpkg at application install time.
# User may edit. LUNA re-reads on every session start.

[[observe.app_rules]]
pattern = "luna-terminal"
context = "CODING"
permissions = ["OBSERVE_APP", "OBSERVE_FILE"]
# observe.toml grants OBSERVE_FILE for luna-terminal specifically.

[[observe.app_rules]]
pattern = "code"
context = "CODING"
permissions = ["OBSERVE_APP", "OBSERVE_FILE"]

[[observe.app_rules]]
pattern = "firefox"
context = "BROWSING"
permissions = ["OBSERVE_APP"]
# Firefox does NOT grant OBSERVE_URL — that requires a separate browser extension.
```

### 2. permissions.db — Runtime-Granted Permissions

```sql
-- ~/.luna/config/permissions.db — SQLite

CREATE TABLE permissions (
    permission_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    category         TEXT NOT NULL,         -- e.g. "EXECUTE_SHELL"
    target           TEXT NOT NULL,         -- e.g. "git push origin main"
    scope            TEXT NOT NULL,         -- "ONCE", "SESSION", "PERMANENT"
    granted_at       INTEGER NOT NULL,      -- Unix timestamp
    expires_at       INTEGER,               -- NULL if PERMANENT or SESSION
    user_confirmed   BOOLEAN NOT NULL,      -- whether user actively confirmed
    notes            TEXT                   -- what LUNA said when requesting
);
```

**Permission scopes:**

| Scope | Meaning |
|---|---|
| `ONCE` | Granted for this single action only. Expires immediately after. |
| `SESSION` | Granted for the duration of this boot session. |
| `PERMANENT` | Granted indefinitely. User must revoke manually. |

---

## The Permission Dialog

When LUNA requests a permission that has not been granted, a **Permission Dialog** appears via luna-island. This is LUNA herself asking — not an OS system dialog.

```
Permission Dialog rendering (inside LUNA Island — COMPACT_PANEL size):

  ╔═══════════════════════════════════════════════════╗
  ║  ●  LUNA — Permission Request                      ║
  ╠═══════════════════════════════════════════════════╣
  ║  To help with your build error, I need to          ║
  ║  read the error log file:                         ║
  ║  ~/.luna/logs/build-error-2026-06-27.log           ║
  ║                                                     ║
  ║  ┌──────────┐  ┌───────────┐  ┌──────────────┐    ║
  ║  │ Just once│  │This session│  │    Always    │    ║
  ║  └──────────┘  └───────────┘  └──────────────┘    ║
  ║                                      [ Deny ]      ║
  ╚═══════════════════════════════════════════════════╝
```

### Dialog Rules

1. **LUNA's language, not system language.** The dialog text is written by the Personality Engine using LUNA's dialogue style. Not: "Permission required to access file system resource." Yes: "To help with your build error, I need to read the error log file."

2. **Always show what is being accessed.** File path, command, URL — always displayed explicitly, never hidden.

3. **Always offer "Just once".** The user should never feel locked in. `ONCE` is always available.

4. **Never request more than needed.** If LUNA needs to read one file, she asks for that one file — not "access to your filesystem."

5. **Deny is always available.** The Deny button is always visible. It never auto-dismisses.

6. **LUNA does not re-ask after three denials.** If the user denies the same permission three times, LUNA stops requesting it for the remainder of the session. She may ask again next session, but only once.

---

## EXECUTE_SHELL — Highest Risk Category

`EXECUTE_SHELL` is the most dangerous permission. LUNA proposing to run a shell command on the user's behalf must follow the strictest rules:

### Rules for EXECUTE_SHELL

```
EXECUTE_SHELL permission rules:

  1. LUNA never constructs a shell command using unsanitized user input.
     Shell commands proposed by LUNA are always from a pre-defined template set.
     Example: "git push origin {branch}" — branch is substituted from git state,
              not from user-provided text.

  2. LUNA never requests EXECUTE_SHELL permission proactively.
     It must be user-initiated (user asks LUNA to run something) or
     a direct response to a specific system event (crash → offer to open log).

  3. The full command is always shown in the permission dialog before execution.
     Never: "Run the fix command?" (vague)
     Always: "Run: git stash && git pull --rebase origin main?"

  4. EXECUTE_SHELL is always ONCE scope. Never SESSION or PERMANENT.
     Every command execution requires a fresh confirmation.

  5. EXECUTE_SHELL commands run in a restricted environment:
     - Current working directory: the user's project root (not /)
     - Environment: minimal (PATH, HOME, USER only)
     - Timeout: 30 seconds (configurable, max 300)
     - No network access (unless separately permitted)
```

### EXECUTE_SHELL Dialog

```
EXECUTE_SHELL dialog:

  ╔══════════════════════════════════════════════════════════╗
  ║  ●  LUNA — Run Command?                                   ║
  ╠══════════════════════════════════════════════════════════╣
  ║  The build has failed 3 times with the same error.        ║
  ║  Want me to run:                                          ║
  ║                                                            ║
  ║  ┌──────────────────────────────────────────────────────┐ ║
  ║  │  $ make clean && make 2>&1 | head -50                │ ║
  ║  └──────────────────────────────────────────────────────┘ ║
  ║                                                            ║
  ║  In: /home/user/lunagui                                   ║
  ║                                                            ║
  ║         [ Run once ]                 [ Deny ]             ║
  ╚══════════════════════════════════════════════════════════╝
```

---

## AppArmor Integration

luna-ai-d itself runs under an AppArmor profile. The Permission Engine is the software enforcement layer, but AppArmor provides the **kernel-level enforcement** that cannot be bypassed even if the Permission Engine has a bug.

```
# AppArmor profile for luna-ai-d (abbreviated):

/usr/bin/luna-ai-d {
    # Memory files — read/write own memory only
    owner @{HOME}/.luna/memory/** rw,

    # Config files — read only
    @{HOME}/.luna/config/** r,

    # Workflow DB — read/write
    owner @{HOME}/.luna/memory/workflow.db rw,

    # Network — only localhost (Ollama)
    network inet stream,
    # Deny all outbound network to non-localhost (enforced at kernel)

    # No access to other users' home directories
    deny /home/*/  r,

    # Execute — only allowed subprocesses (Ollama, TTS)
    /usr/bin/ollama ix,
    /usr/bin/piper ix,  # TTS if enabled

    # No raw device access
    deny /dev/**  rw,
}
```

The AppArmor profile enforces the Permission Engine's decisions at the kernel level. Even if a bug allowed LUNA to bypass a software permission check, AppArmor blocks the actual kernel call.

---

## Permission Audit Log

Every permission decision is logged for user review:

```
~/.luna/logs/permissions.log (human-readable):

2026-06-27 13:05:12  GRANTED   READ_FILE    ~/.luna/logs/build-error.log  [scope: ONCE]
2026-06-27 13:12:44  DENIED    EXECUTE_SHELL  make clean && make           [user denied]
2026-06-27 13:45:01  GRANTED   EXECUTE_SHELL  git push origin main         [scope: ONCE]
2026-06-27 14:00:00  DENIED    OBSERVE_URL   firefox                       [auto-denied: not in observe.toml]
```

Users can view this log via:
```bash
luna permissions log        # Show recent permission decisions
luna permissions log --full # Show all decisions (paginated)
```

---

## Permission Review Interface

Users can review and revoke all permanent permissions:

```bash
luna permissions list
  → Lists all PERMANENT permissions currently granted

luna permissions revoke <permission_id>
  → Revokes a specific permanent permission

luna permissions clear
  → Revokes ALL permanent permissions (luna will ask again as needed)
```

D-Bus equivalents:
```
Service: org.lunaos.luna
Interface: org.lunaos.luna.Permissions

Methods:
  ListPermissions() → array<dict>
  RevokePermission(permission_id: int) → void
  ClearAllPermissions() → void
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Default is deny — LUNA starts with minimum permissions | Core Law II | ✅ Accepted |
| EXECUTE_SHELL is always ONCE scope only | This document | ✅ Accepted |
| EXECUTE_SHELL commands use pre-defined templates only | This document | ✅ Accepted |
| Full command/path shown in permission dialog | This document | ✅ Accepted |
| Stop re-asking after 3 denials (session) | This document | ✅ Accepted |
| AppArmor profile for luna-ai-d | DL-020 | ✅ Accepted |
| Permission audit log at ~/.luna/logs/permissions.log | This document | ✅ Accepted |
| OBSERVE_URL requires separate browser extension | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Browser extension for OBSERVE_URL.** URL-level context requires a browser extension that publishes the active URL to D-Bus. Who builds and maintains this extension? Is it first-party or opt-in community? Must be a Decision Log entry.

2. **Permission dialog visual design.** This document describes the permission dialog conceptually. The exact visual design (typography, button sizes, hover states) must be specified in Volume III / 09_visual_language.md or a dedicated Permission Dialog design document.

3. **EXECUTE_SHELL timeout.** 30-second default, 300-second max. Are these correct? Long builds may need more time. Should the user be able to configure the timeout per-command?

4. **luna-notif permission.** `SEND_NOTIFICATION` is granted by default. Should there be a category for "notification frequency" — e.g., a cap on how many times per hour LUNA can send notifications without a user interaction?

5. **Permission inheritance.** If the user grants PERMANENT `READ_FILE` for a directory, does that extend to subdirectories? Current rule: no inheritance — each file path is a separate permission. This may be too granular for large codebases.

---

## AI Context

- The Permission Engine is the **gatekeeper**. No capability LUNA has should be invoked without first checking the Permission Engine. If you are writing code that reads a file, executes a command, or accesses a network resource inside luna-ai-d, add a permission check before that call.
- `EXECUTE_SHELL` is the highest-risk permission. It should be used extremely sparingly. The templates are fixed for a reason — do not allow arbitrary command construction from user input.
- AppArmor is the backup enforcement layer. The Permission Engine should never rely on AppArmor to catch mistakes — it is defense-in-depth, not a replacement for correct permission checks.
- The audit log is for the user. Write to it on every permission decision, not just denials. The user should be able to see exactly what LUNA did and when.
- Core Law II (Privacy First) is the philosophical source of this entire system. When in doubt, the answer is deny.

---

*Document: `Volume IV / 05_permission_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, non_negotiables.md, DL-020, Core Laws II & V*
*Informs: Volume IV/09_automation.md, Volume V/08_sdk.md*
