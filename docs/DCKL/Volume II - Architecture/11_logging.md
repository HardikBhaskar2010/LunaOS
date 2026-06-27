# Mahina — Logging Architecture
**Volume II · Chapter 11**
**Classification:** Core Architecture — Observability
**Status:** Active · Reference for all component logging implementation

---

## Purpose

This document specifies the Mahina logging architecture: the logging strategy, log file locations, log formats, log levels, log rotation policy, and the rules governing which components log where and what they must log.

This document is the authoritative reference for:
- Implementing logging in any Mahina component
- Diagnosing boot failures and service crashes
- Building log analysis tools (`luna-bootprof`, future log viewer)
- Understanding what is and is not recorded by the system

---

## Overview

Mahina uses a **decentralized file-based logging model**. There is no centralized log daemon (no journald, no syslog-ng, no rsyslog). Each component writes its own log files to defined locations in `/var/log/luna*/` (system components) or `~/.luna/logs/` (user-visible AI logs).

This design is consistent with Core Law I (Own Every Layer) — a central logging daemon is an upstream tool we would not fully control. File-based logs are simple, inspectable with any text tool, and have no hidden behavior.

---

## Design Philosophy

**No centralized log daemon.** Mahina does not run journald, syslog-ng, or rsyslog. Each component writes its own logs. The log format is standardized so that any Mahina log file can be parsed by the same tools.

**Logs are for operators, not metrics.** Mahina logs record events relevant to debugging and diagnostics. They do not record usage metrics, behavioral analytics, or any data that could be used to profile the user. Logging is governed by Core Law V — the user owns everything on the machine, including the logs.

**User-facing AI logs are human-readable.** `~/.luna/logs/luna-ai-d.log` is a log the user may want to inspect to understand what LUNA.AI is doing. It uses plain English descriptions, not opaque codes. System logs may use more technical formats.

**Log files are never transmitted.** No Mahina service reads log files and sends them to a remote server. Crash logs, error logs, and boot logs stay on the machine. The user may share them manually if desired.

---

## Architecture

### Log File Map

```
/var/log/
├── luna-init/
│   ├── boot.log                 # Boot sequence log (Stages 0–7)
│   └── runtime.log              # Post-boot luna-init events (service restarts, etc.)
│
├── luna-ai-d.log                # LUNA.AI daemon system log
│
└── lpkg/
    └── install.log              # Package install/remove/update history

~/.luna/logs/
└── luna-ai-d.log                # User-readable AI behavior log (separate from system log)
```

### Log Format

All Mahina log files use a consistent structured format:

```
[YYYY-MM-DDTHH:MM:SS.mmmZ] [LEVEL] [COMPONENT] message
```

Where:
- `YYYY-MM-DDTHH:MM:SS.mmmZ` — ISO 8601 timestamp with milliseconds, UTC
- `LEVEL` — one of: `FATAL`, `ERROR`, `WARN`, `INFO`, `DEBUG`, `TRACE`
- `COMPONENT` — the component name (e.g., `luna-init`, `luna-ai-d`, `lpkg`)
- `message` — the log message, single line, no embedded newlines

**Boot log timestamps use milliseconds from PID 1 start** (not wall clock), to preserve timing accuracy before the clock is synced:

```
[TIMESTAMP_MS] [STAGE] [COMPONENT] [LEVEL] message
```

Example:

```
[2025-03-15T08:42:01.341Z] [INFO]  [luna-ai-d] LUNA online. Mode: AMBIENT
[2025-03-15T08:42:01.342Z] [DEBUG] [luna-ai-d] Memory store loaded. Patterns: 47
[2025-03-15T08:43:15.891Z] [INFO]  [luna-ai-d] Mode change: AMBIENT → DEVSHELL (terminal focused)
[2025-03-15T08:43:15.892Z] [DEBUG] [luna-ai-d] Observation active for: code, terminal
[2025-03-15T09:12:44.001Z] [WARN]  [luna-ai-d] Pattern confidence below threshold: build_run/code (0.62 < 0.75)
```

---

## Current Decisions

### Log Levels

| Level | Meaning | Examples |
|---|---|---|
| `FATAL` | Process cannot continue — imminent crash | luna-init: failed to mount root filesystem |
| `ERROR` | Significant failure — component may be degraded | luna-ai-d: Ollama connection refused |
| `WARN` | Unexpected condition — system continues normally | Pattern confidence below threshold |
| `INFO` | Normal significant events | LUNA online. Mode change. Service started. |
| `DEBUG` | Detailed operational information | Memory store item count. Query payload. |
| `TRACE` | Extremely detailed — function call level | Per-frame compositor timing. IPC message bytes. |

**Default log levels by component:**

| Component | Default Level | Rationale |
|---|---|---|
| `luna-init` (boot) | `INFO` | Boot log should capture all significant events |
| `luna-init` (runtime) | `WARN` | Post-boot: only record problems |
| `luna-ai-d` (system log) | `INFO` | AI events worth recording |
| `luna-ai-d` (user log) | `INFO` | User-readable, plain English |
| `lpkg` | `INFO` | All installs/removals recorded |
| LGP compositor | `WARN` | Compositor is high-frequency — INFO would flood logs |
| Shell components | `WARN` | Same as compositor |

DEBUG and TRACE logs are only emitted when a component is started with a `--log-level debug` or `--log-level trace` flag. They are never on by default in production.

### Component Logging Requirements

Every Mahina component must log the following events at the specified level:

**luna-init:**

| Event | Level |
|---|---|
| PID 1 start — Mahina version | INFO |
| Each boot stage start and completion | INFO |
| Each service start attempt | INFO |
| Each service ready confirmation | INFO |
| Each service failure (with exit code) | ERROR |
| Each service DEGRADED state entry | ERROR |
| Service restart attempt | WARN |
| Boot complete (with total time) | INFO |
| Any service that fails all restart attempts | FATAL |

**luna-ai-d (system log):**

| Event | Level |
|---|---|
| Daemon start, Ollama connection result | INFO |
| Mode changes (AMBIENT → DEVSHELL, etc.) | INFO |
| Pattern detected (app, type, confidence) | DEBUG |
| Suggestion generated (pattern, confidence) | INFO |
| Suggestion dismissed by user (strike count) | INFO |
| Pattern suppressed by Three-Strike Rule | WARN |
| `luna observe --off` received | INFO |
| `luna memory --clear` received | INFO |
| Cloud bridge call initiated | INFO |
| Ollama connection lost | ERROR |
| Self-test failure at startup | FATAL |

**luna-ai-d (user log at `~/.luna/logs/luna-ai-d.log`):**

The user log is a filtered, human-readable view of AI behavior. It does not contain DEBUG or TRACE entries. It uses plain language:

```
[2025-03-15T08:42:01Z] LUNA came online.
[2025-03-15T08:43:15Z] Switched to developer mode (you opened a terminal).
[2025-03-15T09:15:22Z] Noticed you've run 'make' 4 times in the last hour.
[2025-03-15T09:30:00Z] Pattern suggestion generated: auto-run make before git push.
[2025-03-15T09:30:05Z] Suggestion dismissed.
[2025-03-15T10:45:00Z] Observation paused (luna observe --off).
```

**lpkg:**

| Event | Level |
|---|---|
| Command invoked (install/remove/update) | INFO |
| Package signature verification result | INFO |
| Each file installed/removed | DEBUG |
| Dependency resolution result | INFO |
| Installation complete or failed | INFO / ERROR |
| Conflicting file detected | ERROR |

### Log Rotation

Log files are rotated by a Mahina-provided utility (`luna-logrotate`, or by leveraging `logrotate` as an upstream tool):

```
TODO:
Decision not yet finalized.
Reason: Log rotation tool has not been chosen.
Options:
  A: logrotate — well-understood, standard Linux tool. An upstream dependency
     we would fully understand (Law I permits this).
  B: A minimal custom log rotation script run as a luna-init service.
Recommendation: logrotate as a known, auditable upstream tool.
Must be a Decision Log entry.
```

**Rotation policy (proposed):**

| Log file | Max size | Retention |
|---|---|---|
| `/var/log/luna-init/boot.log` | 10 MB | Last 10 rotations |
| `/var/log/luna-init/runtime.log` | 20 MB | Last 5 rotations |
| `/var/log/luna-ai-d.log` | 50 MB | Last 3 rotations |
| `~/.luna/logs/luna-ai-d.log` | 10 MB | Last 5 rotations |
| `/var/log/lpkg/install.log` | 20 MB | Last 10 rotations (keep full package history) |

Rotated logs are compressed with gzip. Old rotations are deleted automatically.

---

## Technical Details

### Log File Implementation

Components open their log file with `O_WRONLY | O_CREAT | O_APPEND`. Each log entry is a single `write()` syscall (atomic for entries under PIPE_BUF size — 4096 bytes on Linux). No log entry should exceed 4096 bytes.

Log files are not fsynced on every entry. This is a performance tradeoff: entries may be lost if the system crashes immediately after writing. For boot-critical events (`luna-init` Stage 0-2), fsync is used after critical entries. For normal operational logs, it is not.

### Logging from C Components (luna-init)

luna-init implements a minimal logging library:

```c
// luna_log.h
typedef enum {
    LOG_FATAL = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4,
    LOG_TRACE = 5,
} luna_log_level;

void luna_log(luna_log_level level, const char *component, const char *fmt, ...);
```

No external logging library is used in `luna-init`. The implementation is a single C file. The format string is written to the log file using `vsnprintf` followed by `write()`.

### Boot Log vs. Runtime Log

`luna-init` uses two separate log files:

- **boot.log:** Written from PID 1 start through Stage 7 (boot complete). Timestamps are milliseconds from start. After boot, the file is closed and never written to again.
- **runtime.log:** Opened after boot complete. Used for post-boot luna-init events (service restarts, SIGHUP reloads, shutdown sequences). Wall-clock ISO 8601 timestamps.

This separation makes it easy to analyze boot performance without wading through runtime events, and to see runtime events without boot noise.

### Diagnosing Boot Failures

If boot fails at or before Stage 5 (no compositor), the boot log is accessible via:

```sh
# From emergency shell (TTY1)
cat /var/log/luna-init/boot.log

# Or from another OS / recovery mode:
mount /dev/sda1 /mnt
cat /mnt/var/log/luna-init/boot.log
```

If boot fails at Stage 6 or 7, the desktop session starts in degraded mode and the boot log can be inspected via:

```sh
luna-bootprof         # Planned tool — shows per-stage timing
cat /var/log/luna-init/boot.log
```

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| `luna-bootprof` tool | v1 | Parse boot.log, display per-stage timing in a human-readable table |
| `luna logs` CLI | v1 | Unified log viewer: `luna logs ai`, `luna logs boot`, `luna logs pkg` |
| Log viewer in luna-island | v2 | Visual log display in the LUNA interface for non-technical users |
| Structured log format (JSON lines) | v2 | Easier machine parsing; trade-off is less human-readable |
| Log rotation tooling | v1 | Decide and implement log rotation (see TODO) |
| Per-session AI log | v1.5 | One log file per user session rather than one continuous file |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Log rotation tool.** logrotate vs. custom. Must be a Decision Log entry.

2. **LGP compositor log verbosity.** The compositor runs at high frequency (60Hz+). INFO-level logging per frame would generate megabytes per second. WARN default is chosen, but the exact events that warrant WARN in the compositor have not been defined. This must be specified in Volume III / 03_compositor.md.

3. **luna-ai-d user log language.** The user log uses plain English. Locale/language support for non-English users has not been addressed. For v1, English only is the default. Internationalization is post-v1.

4. **Log access permissions.** `/var/log/luna-ai-d.log` — should non-root users read this? Currently: root-only. But the user should be able to see their AI daemon's system log for debugging. A `luna` group with read access is one approach.

5. **Boot log timestamp format.** Boot log uses milliseconds from PID 1 start. Runtime log uses ISO 8601 wall clock. When investigating a crash that spans both logs, correlating timestamps requires knowing when PID 1 started. The wall clock at PID 1 start should be the first INFO entry in boot.log.

---

## AI Context

An AI agent implementing any Mahina component must understand:

- There is no centralized logging daemon. Each component writes its own log file. Do not write logs to syslog or journald — neither exists on Mahina.
- All log files use the format: `[ISO8601_TIMESTAMP] [LEVEL] [COMPONENT] message`. Boot log uses `[MS_FROM_START] [STAGE] [COMPONENT] [LEVEL] message`.
- Log file paths are defined in this document. Do not write logs to undocumented paths.
- Default log level for most components is `WARN`. INFO and DEBUG must be explicitly requested. Verbose logging is never the default in production.
- Log entries must be single-line and under 4096 bytes for atomic write safety.
- Never log user data, AI memory contents, or anything from `~/.luna/memory/`. The user log (`~/.luna/logs/luna-ai-d.log`) logs behavior descriptions, not data contents.
- Log files are never transmitted. No code should read a log file and send it anywhere without explicit user instruction.
- `luna-init` uses a minimal C logging library with no external dependencies. No log crate, no spdlog, no log4j equivalent.

---

*Document: `Volume II / 11_logging.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: architecture_overview.md, boot_flow.md, init_system.md, core_laws.md (Law I, V), non_negotiables.md*
