# Mahina — Memory Engine
**Volume IV · Chapter 4**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · This document specifies how LUNA remembers across sessions

---

## Purpose

This document specifies the **Memory Engine** — the component of `luna-ai-d` responsible for LUNA's persistent memory: what she remembers about the user across sessions, how that memory is maintained, how it is summarized at shutdown, and how it shapes future interactions.

The Memory Engine is what makes LUNA feel like she **knows you**, rather than starting fresh every boot. It is the difference between an AI that says "LUNA online" on day one and day three hundred in exactly the same way, versus one that says "Welcome back. You were debugging layout.c when you left." — because she actually knows that.

All Memory Engine operations are **exclusive to `luna-ai-d`**. No other process touches memory files. This is a non-negotiable privacy boundary (Core Law II).

---

## Overview

```
Memory Engine — data flows:

  ┌────────────────────────────────────────────────────────────┐
  │                     RUNTIME (Session active)               │
  │                                                             │
  │  Presence Engine                                           │
  │  └── workflow.db ←──────── writes app/mode events         │
  │                                                             │
  │  Context Engine                                            │
  │  └── workflow.db ←──────── reads for pattern queries       │
  │                                                             │
  │  Inference Engine                                          │
  │  └── persistent_summary ←── reads working memory          │
  │      on every LLM call                                     │
  └────────────────────────┬───────────────────────────────────┘
                           │  SESSION_ENDED signal (shutdown)
  ┌────────────────────────▼───────────────────────────────────┐
  │                  MEMORY ENGINE (shutdown)                   │
  │                                                             │
  │  1. Read this session's app_events from workflow.db        │
  │  2. Ask LLM: "Summarize this session"                      │
  │     (this is the ONE LLM call Memory Engine makes)         │
  │  3. Merge summary with persistent_summary                  │
  │  4. Encrypt and write to disk                              │
  │  5. Optionally prune old sessions from workflow.db         │
  └────────────────────────┬───────────────────────────────────┘
                           │  NEXT BOOT
  ┌────────────────────────▼───────────────────────────────────┐
  │                  MEMORY ENGINE (startup)                    │
  │                                                             │
  │  1. Decrypt and load persistent_summary                    │
  │  2. Inject summary into Inference Engine working memory    │
  │  3. Presence Engine ready to use pattern data from         │
  │     workflow.db (already open)                             │
  └────────────────────────────────────────────────────────────┘
```

---

## Memory Architecture

LUNA's memory has two distinct layers:

```
Memory layers:

  LAYER 1: WORKING MEMORY (workflow.db — SQLite)
  ───────────────────────────────────────────────
  What:   Raw session data. App events, mode changes, file paths.
          The full unprocessed record of what the user did.
  Scope:  Rolling 90 days. Older sessions are pruned.
  Read:   Context Engine (pattern queries)
  Write:  Presence Engine (event logging)
  Format: SQLite, WAL mode
  Location: ~/.luna/memory/workflow.db
  Encryption: No (file permissions only — only luna-ai-d reads it)

  LAYER 2: PERSISTENT MEMORY (persistent_summary — encrypted file)
  ─────────────────────────────────────────────────────────────────
  What:   LLM-synthesized understanding of the user.
          Projects, preferences, recurring patterns, important events.
          High-signal, low-noise summary of months of working memory.
  Scope:  Indefinite. Never auto-pruned.
  Read:   Inference Engine (injected into every LLM system prompt)
  Write:  Memory Engine (at session shutdown)
  Format: Structured text (Markdown-like, LLM-readable)
  Location: ~/.luna/memory/persistent_summary.enc
  Encryption: Yes — AES-256-GCM (DL-023)
```

---

## Persistent Summary Format

The persistent summary is a structured document that the LLM reads at the start of every conversation. It provides LUNA with continuity across sessions.

```markdown
# LUNA Persistent Memory
Last updated: 2026-06-27

## User Profile
- Primary languages: C, Python
- Main projects: Mahina (lunagui, lgp-compositor, luna-ai-d)
- Preferred shell: bash
- Work schedule: typically 09:00–22:00 IST, breaks infrequent
- Pushes to git: usually between 18:00–20:00

## Recent Projects
- lunagui/layout.c — working on flexbox layout engine (last seen: 2026-06-27)
  Note: encountered flex shrink calculation bug, debugging in progress
- lgp-compositor/render.c — render abstraction layer (last seen: 2026-06-26)

## Recurring Patterns
- Opens Discord briefly when switching contexts (3–4x per session)
- Compiles frequently (build → check → fix cycle, avg 8 min per iteration)
- Long sessions: avg 6.5h, rarely takes breaks

## User Preferences
- Prefers terse LUNA responses
- Has dismissed memory usage warnings 3x — consider raising threshold

## LUNA Observations (flagged by user as useful)
- [2026-06-20] Git reminder was acted on — user pushed
- [2026-06-22] Build failure diff suggestion was used

## Notable Events
- [2026-06-25] lgp-compositor crash (OOM) — Ollama memory pressure
```

This format is designed to be:
- **Readable by the LLM** — structured but not code; natural language summaries
- **Compact** — fits within the LLM's context window alongside the conversation
- **Honest** — only contains things LUNA actually observed, not guesses

### Summary Size Limits

```
Persistent summary size targets:

  Minimum:          1,000 tokens
  Target:           3,000–5,000 tokens
  Maximum:          8,000 tokens (leaves room for conversation in context window)

  When summary exceeds 8,000 tokens:
    → Memory Engine triggers a compression pass
    → LLM is asked to compress the summary to 4,000 tokens
    → Oldest/least-referenced entries are deprioritized for removal
```

---

## Summarization Pass (Shutdown)

When `luna-ai-d` receives the SESSION_ENDED signal (graceful shutdown), the Memory Engine executes a summarization pass:

### Step 1: Read Session Data

```python
def read_current_session(session_id: str) -> list[dict]:
    """
    Query workflow.db for all events in the current session.
    Returns a list of app_events and mode_events.
    """
    conn = sqlite3.connect(WORKFLOW_DB_PATH)
    app_events = conn.execute("""
        SELECT app_name, classified_context, duration_seconds, file_path
        FROM app_events
        WHERE session_id = ?
        ORDER BY timestamp
    """, (session_id,)).fetchall()

    mode_events = conn.execute("""
        SELECT from_mode, to_mode, trigger
        FROM mode_events
        WHERE session_id = ?
        ORDER BY timestamp
    """, (session_id,)).fetchall()

    return { "app_events": app_events, "mode_events": mode_events }
```

### Step 2: Summarize with LLM

```python
SUMMARIZATION_PROMPT = """
You are summarizing a Mahina session for LUNA's persistent memory.
Be extremely concise. Focus on:
1. What the user was working on (project names, file names if significant)
2. Any notable events (errors, crashes, long operations)
3. Any patterns that differ from the user's norm

Existing summary (do not repeat, only add new information):
{existing_summary}

Today's session data:
{session_data}

Produce an updated summary. Remove outdated information. Keep total length under 5000 tokens.
"""
```

### Step 3: Merge and Encrypt

```python
def update_persistent_memory(new_summary: str) -> None:
    """
    Encrypt the updated summary and write to disk.
    Uses AES-256-GCM with a key derived from the user's login credentials (DL-023).
    """
    key = derive_key_from_user_credentials()
    encrypted = aes_256_gcm_encrypt(new_summary.encode('utf-8'), key)
    write_atomic(PERSISTENT_SUMMARY_PATH, encrypted)
    # write_atomic: write to .tmp, then rename — ensures no partial writes
```

---

## Startup: Loading Persistent Memory

```python
def load_persistent_memory() -> str:
    """
    Decrypt and load the persistent summary at luna-ai-d startup.
    Returns the plaintext summary string, or an empty string on first boot.
    """
    if not os.path.exists(PERSISTENT_SUMMARY_PATH):
        return ""  # First boot — no history

    try:
        key = derive_key_from_user_credentials()
        encrypted = read_file(PERSISTENT_SUMMARY_PATH)
        return aes_256_gcm_decrypt(encrypted, key).decode('utf-8')
    except DecryptionError:
        # Key mismatch — user changed password or file is corrupt
        log_error("LUNA memory: decryption failed — starting with empty memory")
        return ""
```

The loaded summary is stored in the Inference Engine's **working memory** and injected into every LLM system prompt via the `{persistent_memory}` template slot.

---

## Memory Control Interface

Users have full control over LUNA's memory (Core Law V — User Owns the Machine):

### D-Bus Interface

```
Service: org.lunaos.luna
Interface: org.lunaos.luna.Memory

Methods:
  GetMemorySummary() → string
    Returns the plaintext persistent summary.
    Requires: user session authentication (not root)

  ClearMemory() → void
    Deletes persistent_summary.enc and clears workflow.db.
    Emits: MemoryCleared() signal when complete.

  ClearSessionMemory(session_id: string) → void
    Removes a specific session from workflow.db.
    Does not affect persistent_summary.

  GetWorkflowStats() → dict
    Returns: session count, total app events, date range in workflow.db.

Signals:
  MemoryCleared()
    Emitted after ClearMemory() completes.
```

### CLI Interface

```bash
# luna memory — user-facing memory management CLI

luna memory status
  → "Persistent memory: 4,234 tokens. Workflow database: 89 sessions (90 days)."

luna memory show
  → Prints the plaintext persistent summary to stdout.

luna memory clear
  → Prompts: "This will permanently delete LUNA's memory of you. Continue? [y/N]"
  → On confirm: calls ClearMemory() via D-Bus

luna memory clear-session <session-id>
  → Removes a specific session from workflow.db

luna memory export
  → Exports the persistent summary as a plaintext Markdown file to ~/luna-memory-export.md
```

---

## Pruning and Retention

```
Data retention rules:

  workflow.db:
    Sessions older than 90 days are pruned.
    Pruning runs during Memory Engine shutdown, after summarization.
    Any patterns from pruned sessions are already captured in persistent_summary.

  persistent_summary.enc:
    Never auto-pruned.
    Grows over time until it exceeds 8,000 tokens.
    At that point, the LLM compresses it (the oldest/least useful parts are dropped).
    The user can manually clear it at any time via `luna memory clear`.

  Crash recovery:
    If luna-ai-d crashes before summarization runs, the session is not summarized.
    The raw app_events remain in workflow.db for the next summarization pass.
    On next startup, Memory Engine checks for sessions with null end_time
    and summarizes them opportunistically (low-priority background task).
```

---

## Encryption Architecture (DL-023)

```
Encryption specification:

  Algorithm:    AES-256-GCM (authenticated encryption)
  Key:          Derived from user login credentials via PBKDF2-HMAC-SHA256
                Salt: stored in ~/.luna/memory/.key_salt (random, 32 bytes)
                Iterations: 100,000
  IV/Nonce:     Random 12 bytes, prepended to ciphertext
  Authentication tag: 16 bytes, appended to ciphertext

  File format:
    [12 bytes nonce] [N bytes ciphertext] [16 bytes auth tag]

  Key rotation:
    If user changes login password, the key changes.
    Old encrypted summary cannot be decrypted without old key.
    On key rotation: Memory Engine detects decryption failure on startup,
    logs the event, and starts with empty memory.
    (TODO: Key rotation with re-encryption is a v1.5 feature — see Open Questions)
```

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| Summarization pass duration (shutdown) | < 30 seconds | 60 seconds |
| Persistent memory load (startup) | < 200ms | 500ms |
| Memory write (encrypted, atomic) | < 100ms | 500ms |
| workflow.db prune pass | < 5 seconds | 15 seconds |
| GetMemorySummary() D-Bus response | < 500ms | 2 seconds |
| Persistent summary size (tokens) | 3,000–5,000 | 8,000 |

**Shutdown timing constraint:** The summarization pass runs during shutdown. luna-init waits for luna-ai-d to exit cleanly. The 30-second target ensures that luna-init's shutdown sequence doesn't time out waiting for the summarization to complete.

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Memory encryption: AES-256-GCM | DL-023 | ✅ Accepted |
| Memory is exclusive to luna-ai-d | Core Law II, Volume IV/00 | ✅ Accepted |
| Persistent summary injected into every LLM call | This document | ✅ Accepted |
| workflow.db retained for 90 days | This document | ✅ Accepted |
| User can clear memory at any time | Core Law V | ✅ Accepted |
| Summarization uses the LLM once per session end | This document | ✅ Accepted |
| Key rotation with re-encryption | Pending — v1.5 | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Key rotation and re-encryption.** If the user changes their login password, the encryption key changes and the old persistent summary cannot be decrypted. v1 loses memory on password change. v1.5 should implement re-encryption with the new key. Requires luna-ai-d to be notified of password changes via PAM or D-Bus. Must be a Decision Log entry.

2. **Multi-user memory isolation.** Each user has their own `~/.luna/memory/`. This is enforced by filesystem permissions. But is there a scenario where luna-ai-d runs as a system service (shared) rather than per-user? No — `luna-ai-d` runs as the logged-in user (Volume IV/00). Confirm this is correct for multi-seat systems.

3. **Memory export format.** `luna memory export` writes to Markdown. Should this be a portable format (JSON, standard Markdown) that could be imported by a future LUNA version or a different system? Must be a Decision Log entry.

4. **Summarization failure handling.** If the LLM call for summarization fails (Ollama crash during shutdown), what happens to the session data? Current answer: raw events remain in workflow.db for the next summarization. Is this sufficient? The next summarization may miss the "last thing the user was doing" context.

5. **LUNA feedback loop.** If the user marks a LUNA observation as unhelpful ("stop suggesting I close Firefox"), should this be recorded in persistent memory so LUNA stops making that suggestion? This is a learning behavior — must be specified.

---

## AI Context

- Memory is the most **privacy-sensitive** component in all of Mahina. Every line of code that touches `workflow.db` or `persistent_summary.enc` must be reviewed with extreme care.
- The persistent summary is injected into every LLM system prompt. Keep it under 8,000 tokens. An oversized summary consumes the LLM's context window and degrades conversation quality.
- Summarization uses the LLM exactly **once per session**, during shutdown. The Memory Engine never calls the LLM during an active session. If you find yourself writing code that calls the LLM from the Memory Engine during a session, you have the architecture wrong.
- The atomic write pattern (`write to .tmp → rename`) is mandatory for all persistent memory writes. A partial write to `persistent_summary.enc` would corrupt it permanently. The rename is atomic on all POSIX filesystems.
- `~/.luna/memory/` is owned by the user. root should never need to read it. If a system process needs to access it, the architecture is wrong.

---

*Document: `Volume IV / 04_memory_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, Volume IV/01_presence_engine.md, Volume IV/03_context_engine.md, DL-023*
*Informs: Volume IV/05_permission_engine.md, Volume V/08_sdk.md*
