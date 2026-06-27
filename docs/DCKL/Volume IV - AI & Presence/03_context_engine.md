# LunaOS — Context Engine
**Volume IV · Chapter 3**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · This document specifies how LUNA understands what the user is doing and computes a confidence score for observations

---

## Purpose

This document specifies the **Context Engine** — the component of `luna-ai-d` that builds a structured understanding of what the user is doing right now, what they have done before, and how confident LUNA should be in any proposed observation or action.

The Context Engine is the bridge between raw data (which app is focused, which file is open) and actionable insight ("the user is debugging a segfault in layout.c — LUNA should offer to check the error log at confidence 0.87").

Without this document, there is no defined method for computing confidence scores or for assembling context into a form the Personality Engine and Inference Engine can use. This is that definition.

---

## Overview

```
Context Engine data flow:

  ┌────────────────────────────────────────────────┐
  │  INPUT SOURCES                                  │
  │                                                  │
  │  Presence Engine context struct                  │
  │  (app, file, mode, session duration)            │
  │                           +                      │
  │  workflow.db (past sessions)                     │
  │  (patterns, preferences, history)               │
  │                           +                      │
  │  System state (CPU, RAM, disk, network)          │
  └──────────────────────┬─────────────────────────┘
                         │
  ┌──────────────────────▼─────────────────────────┐
  │  CONTEXT ENGINE                                  │
  │                                                  │
  │  1. Context Assembler                            │
  │     Combines all inputs into a unified           │
  │     context_snapshot_t struct                   │
  │                                                  │
  │  2. Pattern Matcher                              │
  │     Compares current context to past sessions    │
  │     Detects recurring patterns                   │
  │                                                  │
  │  3. Confidence Scorer                            │
  │     Produces a 0.0–1.0 confidence for any        │
  │     proposed observation or action               │
  │                                                  │
  │  4. Context Publisher                            │
  │     Answers D-Bus queries: GetContext()          │
  │     Publishes context to Inference Engine        │
  └──────────────────────┬─────────────────────────┘
                         │
  ┌──────────────────────▼─────────────────────────┐
  │  OUTPUT CONSUMERS                               │
  │                                                  │
  │  Personality Engine → uses confidence score      │
  │                        to pass/fail Silence Gate │
  │                                                  │
  │  Inference Engine → uses context_snapshot_t      │
  │                      to build the system prompt  │
  │                                                  │
  │  luna-island → uses GetContext() for COMPACT_PANEL│
  └────────────────────────────────────────────────┘
```

---

## Data Structures

### Context Snapshot

The `context_snapshot_t` is the unified view of the user's current state. It is assembled continuously and updated whenever any input source changes.

```c
typedef struct context_snapshot {

    // === Current Activity ===
    char     app_name[128];          // e.g. "luna-terminal"
    char     app_class[64];          // TERMINAL, BROWSER, EDITOR, GAME, etc.
    char     active_file[512];       // e.g. "/home/user/lunagui/layout.c"
    char     file_extension[16];     // e.g. ".c"
    char     project_root[512];      // e.g. "/home/user/lunagui" (git root if detected)
    char     project_name[128];      // e.g. "lunagui" (directory name of project_root)
    uint32_t seconds_in_current_app; // how long the current app has been focused
    uint32_t seconds_in_current_file;// how long the current file has been active

    // === Session ===
    uint64_t session_start;          // Unix timestamp
    uint32_t session_duration_s;     // seconds since session start
    uint32_t apps_opened_today;      // count of distinct apps opened this session
    uint32_t breaks_taken;           // count of idle periods ≥ 5 min

    // === System State ===
    float    cpu_usage_percent;      // system-wide CPU (0–100)
    float    ram_used_gb;            // currently used RAM
    float    ram_total_gb;           // total available RAM
    float    disk_used_percent;      // root filesystem used percent
    bool     network_connected;      // whether a network interface is up
    bool     battery_present;        // whether running on battery
    float    battery_percent;        // battery level (if present)

    // === Pattern Context ===
    float    git_push_expected;      // 0.0–1.0: probability user will push to git soon
    uint32_t minutes_since_last_break; // for break reminder logic
    bool     is_repeating_error;     // same error seen before in this session
    uint32_t error_repeat_count;     // how many times this error has appeared
    char     most_used_app_today[128]; // app with most focus time today

    // === Confidence ===
    float    context_confidence;     // overall confidence in this context snapshot

    // === Timestamp ===
    uint64_t snapshot_time;          // Unix timestamp when this was assembled

} context_snapshot_t;
```

---

## Context Assembler

The Context Assembler combines all input sources into a `context_snapshot_t` on a periodic update cycle.

### Update Cycle

```
Update triggers:

  IMMEDIATE update (within 100ms):
    - LGP_FOCUS_CHANGED event received
    - System RAM crosses a threshold (> 85% used)
    - CPU sustained at > 90% for > 10 seconds
    - Disk usage crosses 90%
    - Network connectivity changes

  PERIODIC update (every 30 seconds):
    - Pattern context refresh (git_push_expected, break probability)
    - System stats update (CPU, RAM, disk)
    - workflow.db pattern query
```

### Project Root Detection

When a file is open, the Context Engine attempts to determine the project root:

```c
char* detect_project_root(const char* file_path) {
    // Walk up the directory tree looking for:
    //   .git directory  → git project
    //   luna.toml       → LunaOS project
    //   Cargo.toml      → Rust project
    //   Makefile        → C/C++ project
    //   package.json    → Node.js project
    // Returns the first matching parent directory.
    // Returns NULL if no project root detected.
}
```

The project root is used to:
- Group multiple files in the same project under one context
- Detect build failures in the context of a known project
- Provide the project name in context snapshots

---

## Pattern Matcher

The Pattern Matcher queries `workflow.db` to find recurring patterns in the user's past behavior. This is the source of LUNA's time-aware observations.

### Pattern Types

**1. Time-of-day patterns**

```sql
-- "Does the user usually do X at approximately this time of day?"
SELECT
    AVG(CAST(strftime('%H', datetime(timestamp, 'unixepoch')) AS INTEGER)) as avg_hour,
    COUNT(*) as occurrences,
    app_name
FROM app_events
WHERE app_name = :app_name
  AND duration_seconds > 300  -- must have used it for > 5 minutes
GROUP BY app_name
HAVING COUNT(*) >= 3  -- must have done it at least 3 times
```

**2. App reopening patterns**

```sql
-- "Has the user opened this app multiple times in a short window?"
SELECT COUNT(*) as open_count
FROM app_events
WHERE app_name = :app_name
  AND timestamp > (strftime('%s', 'now') - 600)  -- in the last 10 minutes
  AND session_id = :current_session_id
```

**3. Git push patterns**

```sql
-- "Does the user usually push to git at this time?"
-- (requires luna-terminal to publish git commands to D-Bus, or file watcher on .git/ORIG_HEAD)
SELECT
    AVG(CAST(strftime('%H%M', datetime(timestamp, 'unixepoch')) AS INTEGER)) as avg_push_hhmm
FROM app_events
WHERE classified_context = 'CODING'
  AND file_path LIKE '%.git%'  -- simplified — real implementation watches git socket
HAVING COUNT(*) >= 5
```

**4. Break patterns**

```sql
-- "How long has the user been working without a break?"
-- (break = idle event in mode_events)
SELECT MAX(timestamp) as last_break
FROM mode_events
WHERE to_mode = 'AMBIENT'
  AND from_mode IN ('DEVSHELL', 'FOCUS', 'STUDY')
  AND session_id = :current_session_id
  AND (timestamp - (SELECT start_time FROM sessions WHERE session_id = :current_session_id)) > 300
```

---

## Confidence Scorer

The Confidence Scorer is the component the Personality Engine depends on to pass or fail the Silence Gate. It produces a `float` between 0.0 and 1.0 representing how certain LUNA should be about a proposed observation.

### Confidence Factors

Confidence for any proposed observation is computed as:

```
confidence = base_score × pattern_multiplier × freshness_multiplier × coherence_multiplier
```

**Base score** — how strong the raw signal is:

| Situation | Base Score |
|---|---|
| System event (crash, OOM, disk full) | 1.0 — always certain |
| Error repeated 3+ times in session | 0.95 |
| App opened 3+ times in 10 minutes | 0.85 |
| Time-of-day pattern (5+ past occurrences) | 0.80 |
| Time-of-day pattern (3–4 occurrences) | 0.70 |
| Long session (> 3 hours) without break | 0.75 |
| Single data point observation | 0.40 |

**Pattern multiplier** — does past data support this?

| Pattern data available | Multiplier |
|---|---|
| ≥ 10 historical occurrences | 1.0 |
| 5–9 historical occurrences | 0.9 |
| 3–4 historical occurrences | 0.8 |
| 1–2 historical occurrences | 0.6 |
| No historical data | 0.5 |

**Freshness multiplier** — how recent is the context data?

| Data age | Multiplier |
|---|---|
| < 30 seconds | 1.0 |
| 30s – 5 minutes | 0.9 |
| 5 – 30 minutes | 0.75 |
| > 30 minutes | 0.5 |

**Coherence multiplier** — does the context make sense together?

| Coherence check | Multiplier |
|---|---|
| All signals agree | 1.0 |
| Minor conflict in signals | 0.85 |
| Major conflict (e.g., user in GAMING mode but receiving CODING observations) | 0.3 |

### Example Confidence Calculations

```
Example 1: Git push reminder
  Base:        0.80 (time-of-day pattern, 6 occurrences)
  Pattern:     0.9  (5–9 historical occurrences)
  Freshness:   1.0  (context updated 5 seconds ago)
  Coherence:   1.0  (user is in DEVSHELL, context matches)
  Confidence:  0.80 × 0.9 × 1.0 × 1.0 = 0.72
  Mode:        DEVSHELL (threshold: 0.90)
  Result:      SILENCE GATE BLOCKS — confidence 0.72 < 0.90

Example 2: Firefox memory warning
  Base:        1.0  (system event — memory threshold crossed)
  Pattern:     1.0  (not pattern-dependent — direct measurement)
  Freshness:   1.0  (just measured)
  Coherence:   1.0  
  Confidence:  1.0
  Mode:        AMBIENT
  Result:      SILENCE GATE PASSES — 1.0 ≥ 0.65

Example 3: App reopened too many times
  Base:        0.85 (app opened 4 times in 10 minutes)
  Pattern:     0.8  (3–4 historical occurrences)
  Freshness:   1.0
  Coherence:   1.0
  Confidence:  0.85 × 0.8 × 1.0 × 1.0 = 0.68
  Mode:        AMBIENT (threshold: 0.65)
  Result:      SILENCE GATE PASSES — 0.68 ≥ 0.65
```

---

## Context Publisher

The Context Publisher answers D-Bus queries and provides context to internal components.

### D-Bus Interface

```
Service: org.lunaos.luna
Interface: org.lunaos.luna.Context

Methods:
  GetContext() → dict
    Returns the current context_snapshot_t as a D-Bus dictionary.
    Used by: luna-island (COMPACT_PANEL display), luna-ai-d internal components

  GetConfidence(observation: string) → double
    Returns the confidence score for a specific proposed observation.
    Used by: Personality Engine Silence Gate

Signals:
  ContextChanged(changed_fields: array<string>)
    Emitted when significant fields in the context snapshot change.
    luna-island may subscribe to update its COMPACT_PANEL display.
```

### Context Fields Exposed via GetContext()

The D-Bus `GetContext()` response exposes a **filtered subset** of `context_snapshot_t`:

```python
# Fields exposed via D-Bus (public context)
PUBLIC_CONTEXT_FIELDS = [
    "app_name",
    "app_class",
    "project_name",          # NOT the full file path — only the project name
    "session_duration_s",
    "current_mode",
    "minutes_since_last_break",
    "network_connected",
    "battery_percent",       # only if battery_present
]

# Fields NOT exposed via D-Bus (private context — internal only)
PRIVATE_CONTEXT_FIELDS = [
    "active_file",           # full file path — privacy sensitive
    "project_root",          # full path — privacy sensitive
    "git_push_expected",     # internal pattern — not for external consumers
    "is_repeating_error",    # internal — shared with Inference Engine only
    "context_confidence",    # internal scoring — not exposed
]
```

**Privacy rule:** The full file path and project root are **never exposed via D-Bus**. They are used internally by the Inference Engine (to build the system prompt) and by the Personality Engine (to generate template responses). No external process receives the user's full file paths.

---

## System State Monitor

The Context Engine includes a lightweight System State Monitor that polls system resources and updates the context snapshot:

```c
void update_system_state(context_snapshot_t *ctx) {
    // CPU: read /proc/stat (sample twice, 100ms apart, compute delta)
    ctx->cpu_usage_percent = read_cpu_usage();

    // RAM: read /proc/meminfo
    ctx->ram_used_gb  = read_ram_used_gb();
    ctx->ram_total_gb = read_ram_total_gb();

    // Disk: statvfs() on "/"
    ctx->disk_used_percent = read_disk_percent("/");

    // Network: check if any non-loopback interface has an IP address
    ctx->network_connected = check_network_up();

    // Battery: read /sys/class/power_supply/BAT0/ if present
    ctx->battery_present = file_exists("/sys/class/power_supply/BAT0/");
    if (ctx->battery_present)
        ctx->battery_percent = read_battery_percent();
}
```

**Thresholds that trigger immediate context update (and potentially CRISIS mode):**

| Metric | CRISIS Threshold |
|---|---|
| RAM used | > 90% of total |
| CPU sustained | > 95% for > 30 seconds |
| Disk used | > 95% |
| Battery | < 10% and discharging |

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| Context snapshot update (immediate trigger) | < 10ms | 25ms |
| Context snapshot update (periodic, 30s cycle) | < 50ms | 100ms |
| Pattern query against workflow.db | < 5ms | 20ms |
| Confidence score computation | < 2ms | 10ms |
| GetContext() D-Bus response | < 20ms | 50ms |
| RAM usage (Context Engine data) | < 10 MB | 20 MB |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Full file paths are not exposed via D-Bus | This document (privacy) | ✅ Accepted |
| Confidence score uses multiplicative factor model | This document | ✅ Accepted |
| System state polled every 30 seconds (periodic) | This document | ✅ Accepted |
| Immediate updates on threshold crossings | This document | ✅ Accepted |
| Project root detection uses git / build file markers | This document | ✅ Accepted |
| CRISIS thresholds: RAM > 90%, disk > 95%, battery < 10% | This document | 🧪 Experimental |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Browser URL observation.** For URL-based context (CODING on GitHub vs. AMBIENT on social media), the browser would need to publish its active URL to D-Bus. Is this an opt-in per-browser integration or does LunaOS provide a browser extension? Must be a Decision Log entry.

2. **Git command observation.** Git push pattern detection currently depends on file watcher heuristics (watching `.git/ORIG_HEAD`). A proper integration would have the terminal publish git commands to D-Bus. Is this in scope for v1? Must be a Decision Log entry.

3. **CRISIS threshold calibration.** RAM > 90%, disk > 95%, battery < 10% are estimates. These need to be validated against real hardware behavior. Experimental until tested.

4. **Error repeat detection.** How does the Context Engine detect that a build error is the "same error as yesterday"? This requires error pattern matching across sessions. The simplest approach: normalize the error string (strip line numbers), hash it, compare against `workflow.db`. Must be specified before DEVSHELL observation features are implemented.

5. **Confidence decay.** Should confidence scores decay over time if the user ignores LUNA's observations? If LUNA suggested "Close Firefox?" and the user ignored it three times, should LUNA stop suggesting it? This is a learning behavior and would need Memory Engine support.

---

## AI Context

- The Context Engine is the **source of truth** for what the user is doing. When the Inference Engine builds a system prompt, it gets the context from here via the `context_snapshot_t` struct — not from the Presence Engine directly.
- Full file paths and project roots are **internal only**. They never leave `luna-ai-d`. Do not add code that exposes file paths via D-Bus, log files, or any external interface.
- The confidence score is the mechanism that prevents LUNA from speaking noise. If a new observation feature is added that bypasses the confidence scorer, it will cause LUNA to be annoying. Every proposed observation must go through the scorer.
- System state is polled — not event-driven (except for threshold crossings). Polling every 30 seconds is a deliberate choice to keep the CPU budget near zero during idle.
- The Pattern Matcher queries SQLite (workflow.db). These queries must complete in < 5ms. If a pattern query takes longer, it is blocking the context update and the user will perceive a slow LUNA response. Keep queries simple.

---

*Document: `Volume IV / 03_context_engine.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, Volume IV/01_presence_engine.md, Volume IV/02_personality_engine.md*
*Informs: Volume IV/04_memory_engine.md, Volume IV/06_conversation_rules.md*
