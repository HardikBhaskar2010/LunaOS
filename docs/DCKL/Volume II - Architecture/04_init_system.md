# Mahina — Init System
**Volume II · Chapter 4**
**Classification:** Core Architecture — System Initialization
**Status:** Active · Reference for luna-init implementation

---

## Purpose

This document specifies `luna-init` — the Mahina PID 1 init system. It defines the service model, service file format, supervision behavior, shutdown sequencing, and the interface available to system administrators and AI coding agents.

This document is the authoritative reference for:
- Implementing `luna-init` in C
- Writing service files for Mahina components
- Understanding how services are started, supervised, and stopped
- Debugging service failures at the init level

---

## Overview

`luna-init` is the first process started by the Linux kernel after the initramfs handoff. It holds PID 1 for the entire session. It is responsible for:

1. Mounting filesystems (Stage 2 of the boot sequence — see `02_boot_flow.md`)
2. Running early system hooks (Stage 3)
3. Starting and supervising system services (Stage 4–6)
4. Reaping zombie processes
5. Coordinating orderly system shutdown and reboot
6. Providing a control interface (`luna-init-ctl`) for runtime service management

`luna-init` does not implement a shell, a socket activation daemon, a cron scheduler, or a logging daemon. Each of those concerns belongs to a separate, dedicated service.

---

## Design Philosophy

### Why a Custom Init (Law I)

Per Decision Log DL-002 and Core Law I (Own Every Layer): `luna-init` is written from scratch in C. The alternatives were rejected for the following reasons:

| Alternative | Reason Rejected |
|---|---|
| systemd | Massive complexity; violates Law I — cannot be fully understood at every layer |
| OpenRC | Shell-based; inherits decisions we didn't make |
| runit | Excellent, but still someone else's init |
| s6 | Excellent supervision model, but we'd be configuring a tool we didn't write |

Writing `luna-init` means Mahina owns PID 1 completely. Every behavior is documented here. Every bug is ours. This is the explicit tradeoff of Law I.

### Minimal PID 1 Scope

`luna-init` is not a service manager in the systemd sense. It does not:
- Parse unit files with hundreds of directives
- Implement D-Bus activation
- Manage user sessions
- Handle login management
- Provide a journal / log aggregator

It starts services. It watches them. It restarts them if they die. It shuts them down cleanly. Nothing else.

### Fail-Fast at PID 1

If `luna-init` itself encounters an unrecoverable error (kernel assertion failure, OOM, corrupted service file preventing any service from starting), it drops to an emergency shell on TTY1 rather than silently hanging. A hung PID 1 is unacceptable — the user must always have a way out (Core Law V).

---

## Architecture

### Process Tree

```
PID 1: luna-init
    │
    ├── dbus-daemon
    │     └── (D-Bus clients connect as needed)
    │
    ├── NetworkManager
    │     └── (network management children)
    │
    ├── pipewire
    │     └── wireplumber (session manager)
    │
    ├── ollama
    │     └── (model inference processes)
    │
    ├── lgp-compositor        [name TBD — see TODO in architecture_overview.md]
    │     └── (compositor render threads)
    │
    ├── luna-shell
    ├── luna-bar
    ├── luna-island
    ├── luna-notif
    │
    └── luna-ai-d
          └── (luna-ai-d worker threads)
```

All processes in the tree are direct children of luna-init or children of a luna-init-supervised service. No service spawns untracked children. `luna-init` reaps all orphaned processes via the standard PID 1 zombie reaping contract.

### luna-init Internal Components

```
┌─────────────────────────────────────────────────────┐
│                   luna-init (PID 1)                  │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  │
│  │ Boot Stages  │  │   Service    │  │ Zombie   │  │
│  │ 0 → 7       │  │  Supervisor  │  │ Reaper   │  │
│  └──────────────┘  └──────┬───────┘  └──────────┘  │
│                           │                         │
│  ┌──────────────┐  ┌──────▼───────┐  ┌──────────┐  │
│  │  Shutdown    │  │  Dependency  │  │  luna-   │  │
│  │  Sequencer   │  │    Graph     │  │ init-ctl │  │
│  └──────────────┘  └──────────────┘  └──────────┘  │
└─────────────────────────────────────────────────────┘
```

| Component | Responsibility |
|---|---|
| Boot Stages | Executes the 7-stage boot sequence from `02_boot_flow.md` |
| Service Supervisor | Watches running services, applies restart policy on failure |
| Zombie Reaper | `waitpid(-1, ...)` loop — reaps all orphaned child processes |
| Dependency Graph | Builds start/stop order from service file `depends` declarations |
| Shutdown Sequencer | Stops services in reverse dependency order, with timeout |
| luna-init-ctl | Unix socket server for runtime commands from `luna-init-ctl` CLI |

---

## Service File Format

Service files are TOML documents stored in `/etc/luna/services/`. Each file describes one service.

### Full Service File Specification

```toml
# /etc/luna/services/example.toml

[service]
# Required fields
name        = "example"                    # Canonical service name. Must be unique.
binary      = "/usr/bin/example"           # Absolute path to executable.
description = "Example Mahina service"    # One-line human description.

# Optional: arguments passed to the binary
args = ["--config", "/etc/luna/example.toml"]

# Optional: working directory (default: /)
workdir = "/var/lib/example"

# Optional: environment variables
[service.env]
LUNA_SERVICE = "example"
LOG_LEVEL    = "info"

# Optional: run as a specific user (default: root — avoid when possible)
[service.identity]
user  = "luna"
group = "luna"

# Required: dependency declarations
[service.depends]
after  = ["dbus"]       # Start after these services are RUNNING
before = []             # Start before these services (declares ordering)

# Required: restart policy
[service.restart]
policy   = "on-failure"     # never | always | on-failure
attempts = 3                # Max restart attempts before marking DEGRADED
delay_ms = 1000             # Wait between restart attempts (milliseconds)

# Optional: readiness detection
[service.ready]
# Method: none | file | socket | http | signal
method  = "file"
# For method = "file": luna-init waits for this file to exist
path    = "/var/run/example.ready"
# timeout_ms: how long to wait for readiness before marking service as DEGRADED
timeout_ms = 5000

# Optional: shutdown behavior
[service.stop]
signal     = "SIGTERM"    # Signal to send when stopping (default: SIGTERM)
timeout_ms = 3000         # Time to wait before escalating to SIGKILL
```

### Readiness Methods

| Method | Behavior |
|---|---|
| `none` | luna-init assumes the service is ready immediately after exec() |
| `file` | luna-init polls for the existence of a file at the specified path |
| `socket` | luna-init attempts a connect() to a Unix socket at the specified path |
| `http` | luna-init performs an HTTP GET to a localhost URL; ready when 200 OK |
| `signal` | luna-init waits for the service to send SIGUSR1 to PID 1 |

### Restart Policies

| Policy | Behavior |
|---|---|
| `never` | Service is never restarted after exit. Used for one-shot tasks. |
| `always` | Service is always restarted after exit, regardless of exit code. |
| `on-failure` | Service is restarted only when exit code is non-zero. |

After `attempts` failures in sequence, the service is marked `DEGRADED`. It is not restarted further until the user issues `luna-init-ctl restart <name>` or reboots.

### Reference Service Files

**D-Bus:**

```toml
[service]
name        = "dbus"
binary      = "/usr/bin/dbus-daemon"
description = "D-Bus system message bus"
args        = ["--system", "--nofork", "--nopidfile"]

[service.depends]
after = []

[service.restart]
policy   = "always"
attempts = 5
delay_ms = 500

[service.ready]
method     = "socket"
path       = "/run/dbus/system_bus_socket"
timeout_ms = 3000

[service.stop]
signal     = "SIGTERM"
timeout_ms = 2000
```

**NetworkManager:**

```toml
[service]
name        = "networkmanager"
binary      = "/usr/sbin/NetworkManager"
description = "Network management daemon"
args        = ["--no-daemon"]

[service.depends]
after = ["dbus"]

[service.restart]
policy   = "on-failure"
attempts = 3
delay_ms = 1000

[service.ready]
method     = "file"
path       = "/var/run/NetworkManager/NetworkManager.pid"
timeout_ms = 5000

[service.stop]
signal     = "SIGTERM"
timeout_ms = 3000
```

**Ollama:**

```toml
[service]
name        = "ollama"
binary      = "/usr/bin/ollama"
description = "Local AI model inference server"
args        = ["serve"]

[service.identity]
user  = "luna"
group = "luna"

[service.depends]
after = []

[service.restart]
policy   = "on-failure"
attempts = 3
delay_ms = 2000

[service.ready]
method     = "http"
path       = "http://localhost:11434/"
timeout_ms = 10000

[service.stop]
signal     = "SIGTERM"
timeout_ms = 5000
```

**luna-ai-d:**

```toml
[service]
name        = "luna-ai-d"
binary      = "/usr/bin/luna-ai-d"
description = "LUNA AI daemon"

[service.identity]
user  = "luna"
group = "luna"

[service.depends]
after = ["ollama"]

[service.restart]
policy   = "on-failure"
attempts = 3
delay_ms = 1000

[service.ready]
method     = "http"
path       = "http://localhost:7734/status"
timeout_ms = 10000

[service.stop]
signal     = "SIGTERM"
timeout_ms = 3000
```

---

## Technical Details

### Dependency Graph Resolution

`luna-init` builds a directed acyclic graph (DAG) from the `after` and `before` fields across all service files. The graph is validated at boot time before any service is started.

Validation checks:
1. All referenced service names exist as files in `/etc/luna/services/`
2. No circular dependencies exist (detected via topological sort — DFS with cycle detection)
3. No duplicate service names exist

If validation fails, luna-init logs the error and drops to the emergency shell. Malformed service files must be corrected before boot can proceed.

```
Start order resolution:

Service files loaded
    │
    ▼
Build dependency graph
    │
    ▼
Topological sort (Kahn's algorithm)
    │
    ▼
Start services in sorted order
Parallelize where no shared dependency edge exists
```

### Service States

Each service tracked by luna-init is in one of these states:

| State | Meaning |
|---|---|
| `PENDING` | Not yet started — waiting for dependencies |
| `STARTING` | Process has been exec()'d — waiting for readiness signal |
| `RUNNING` | Service is running and ready |
| `DEGRADED` | Service has exceeded restart attempts — will not be restarted automatically |
| `STOPPING` | Stop signal sent — waiting for process to exit |
| `STOPPED` | Process has exited cleanly |
| `FAILED` | Process exited with error; DEGRADED applies after attempts exhausted |

State transitions:

```
PENDING → STARTING → RUNNING → STOPPING → STOPPED
                ↓
            FAILED → STARTING (restart if policy allows)
                ↓
            DEGRADED (after attempts exhausted)
```

### Zombie Reaping

`luna-init` runs a continuous `waitpid(-1, WNOHANG, ...)` loop as part of its main event loop. Every exited child process (including orphans adopted by PID 1) is reaped immediately. The service supervisor is notified when a supervised service's PID exits.

This is a mandatory PID 1 responsibility. Failure to reap zombies produces process table exhaustion on long-running systems.

### Shutdown Sequence

When `luna-init` receives SIGTERM, SIGINT (from reboot), or a shutdown command via `luna-init-ctl shutdown`:

```
1. Mark system as shutting down — no new services started
2. Determine stop order (reverse of start dependency order)
3. For each service in stop order:
   a. Send configured stop signal (default: SIGTERM)
   b. Wait up to stop.timeout_ms for process to exit
   c. If still running: send SIGKILL
   d. Reap the process
4. Unmount filesystems in reverse mount order
5. Call reboot(2) syscall with appropriate command
   (LINUX_REBOOT_CMD_POWER_OFF or LINUX_REBOOT_CMD_RESTART)
```

Total shutdown target: under 5 seconds. A service that does not respond to SIGTERM within its timeout is killed immediately — it does not block the shutdown.

### Control Interface — luna-init-ctl

`luna-init-ctl` is a CLI tool that communicates with luna-init via a Unix domain socket at `/run/luna-init.sock`. It is the only supported interface for runtime service management.

Commands:

```
luna-init-ctl status                  # Show status of all services
luna-init-ctl status <name>           # Show status of one service
luna-init-ctl start <name>            # Start a service (must be STOPPED or DEGRADED)
luna-init-ctl stop <name>             # Stop a running service
luna-init-ctl restart <name>          # Stop then start a service
luna-init-ctl reload <name>           # Send SIGHUP to a running service
luna-init-ctl shutdown                # Initiate orderly system shutdown
luna-init-ctl reboot                  # Initiate orderly system reboot
luna-init-ctl list                    # List all known services and their states
```

The socket protocol is a simple newline-delimited JSON request/response format:

```json
// Request
{"command": "status", "service": "luna-ai-d"}

// Response
{"service": "luna-ai-d", "state": "RUNNING", "pid": 1234, "uptime_ms": 45231}
```

```
TODO:
Decision not yet finalized.
Reason: Socket protocol format (JSON vs. TOML vs. binary) has not been formally decided.
JSON is used above as a reasonable default for readability.
This decision must be recorded in the Decision Log before implementation.
```

### Signal Handling in luna-init

| Signal | luna-init behavior |
|---|---|
| `SIGCHLD` | Triggers zombie reap loop + service supervisor notification |
| `SIGTERM` | Initiates orderly shutdown |
| `SIGINT` | Initiates orderly shutdown (sent by kernel on Ctrl+Alt+Del if configured) |
| `SIGHUP` | Reload service file definitions from disk (new files picked up, changed files applied) |
| `SIGUSR1` | Dump internal state to `/var/log/luna-init/state-dump.log` — debug aid |

### luna-init Binary

- Written in C (C11 or later).
- Statically linked — no shared library dependencies.
- Must fit in the initramfs with room for other tools.
- No memory allocator beyond the C standard library. No embedded scripting.
- All logging goes to `/var/log/luna-init/boot.log` (during boot) and `/var/log/luna-init/runtime.log` (post-boot).

Implementation requirements:
- `epoll` or `select` for event loop (no external event loop library)
- `signalfd` for signal handling in the event loop (avoids signal handler race conditions)
- `inotify` for watching `/etc/luna/services/` for file changes on SIGHUP
- `fork` + `execve` for service spawning
- `waitpid` for zombie reaping

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Service file hot-reload | v1 | SIGHUP triggers re-read without full restart |
| Per-service cgroup assignment | v1 | Assign each service to a cgroup for resource accounting |
| Service sandboxing primitives | v1.5 | Namespace + seccomp filter per service, declared in service file |
| Socket activation | v2 | Start a service only when its socket receives a connection |
| luna-init-ctl tab completion | v1 | Shell completion for all commands |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **luna-init-ctl socket protocol.** JSON, TOML, or a binary format? Must be a Decision Log entry before implementation.

2. **LGP compositor service file name.** The compositor process name is not yet decided (referred to as `lgp-compositor` as a placeholder). Once Volume III / 01_lgp.md names the compositor binary, a service file must be written and added to the reference set.

3. **Privilege separation model.** Which services run as root vs. the `luna` user vs. dedicated per-service users? The reference service files above use `user = "luna"` for `ollama` and `luna-ai-d`. A complete privilege matrix for all Mahina services is needed before v1.

4. **Emergency shell binary.** The binary dropped to on Stage 1/2 failure has not been decided. Must be statically linked and fit in the initramfs. See `02_boot_flow.md` Open Question 4.

5. **Cgroup v2 integration.** Whether luna-init directly manages cgroup v2 hierarchies (assigning each service to a cgroup) or delegates to a separate cgroup manager is undecided.

---

## AI Context

An AI agent implementing `luna-init` must understand:

- PID 1 is responsible for zombie reaping at all times. If luna-init's `waitpid` loop is not running, the system will accumulate zombie processes. This is a correctness requirement, not a feature.
- Service files are TOML, in `/etc/luna/services/`. No other format is accepted.
- The dependency graph must be validated (including cycle detection) before any service is started. A cycle in service dependencies is a fatal boot error.
- The `DEGRADED` state is a non-restart state. A service in DEGRADED state will not be restarted by luna-init without an explicit `luna-init-ctl restart` command. This is intentional — a service that keeps crashing should not restart in a loop consuming resources.
- luna-init is statically linked. No dynamic libraries. No external dependencies beyond the Linux kernel syscall interface.
- The LGP compositor service file uses the placeholder name `lgp-compositor`. When the actual binary name is decided in Volume III, update the service file accordingly.
- `luna observe --off` (Core Law IV) stops `luna-ai-d`'s observation modules. This is implemented inside `luna-ai-d` itself — luna-init does not participate. luna-init only starts and supervises the process.
- Shutdown must complete within 5 seconds. Services that do not respond to SIGTERM within their configured timeout are killed with SIGKILL. There is no exception to this rule.

---

*Document: `Volume II / 04_init_system.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: architecture_overview.md, boot_flow.md, core_laws.md, decision_log.md (DL-002, DL-008), non_negotiables.md*
