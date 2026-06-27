# Mahina — IPC Architecture
**Volume II · Chapter 7**
**Classification:** Core Architecture — Inter-Process Communication
**Status:** Active · Reference for all service and component implementation

---

## Purpose

This document specifies the inter-process communication (IPC) architecture of Mahina. It defines which IPC mechanisms exist, which components use each mechanism, the protocol formats in use, and the rules governing when a new IPC channel may be introduced.

This document is the authoritative reference for any developer or AI coding agent connecting two Mahina components together.

---

## Overview

Mahina uses multiple IPC mechanisms, each selected for a specific class of communication. No single IPC bus handles all communication. Each mechanism has a documented scope.

| Mechanism | Scope | Format |
|---|---|---|
| D-Bus (system bus) | System-level events between services | D-Bus wire protocol |
| Unix domain sockets | Point-to-point service control | JSON (newline-delimited) |
| HTTP (localhost only) | LUNA.AI queries, shell ↔ AI communication | JSON over HTTP/1.1 |
| Ollama REST API | luna-ai-d ↔ Ollama (model inference) | JSON over HTTP/1.1 |
| PipeWire socket | Audio/video routing | PipeWire native protocol |
| luna-init-ctl socket | luna-init control | JSON over Unix socket |
| LGP protocol | Shell ↔ compositor | TODO — see Volume III |
| Shared memory | High-bandwidth compositor buffer sharing | TODO — depends on LGP |

---

## Design Philosophy

### Explicit Over Implicit

Every IPC connection in Mahina is documented here. If two components communicate through an undocumented channel, it is a violation of Core Law I (Own Every Layer) and Core Law VI (Documentation Is Code). No implicit file-watching side-channels, no polling of shared state through the filesystem except where explicitly documented.

### Localhost Isolation

All HTTP-based IPC is bound to localhost only (`127.0.0.1`). No Mahina service exposes an IPC endpoint on a network interface. The firewall default-blocks all Mahina IPC ports from external interfaces (see `08_security.md`).

### Minimal Trust

Components communicate only with the components they need to. `luna-ai-d` does not have a D-Bus connection. The compositor does not connect to Ollama. Each component's IPC surface is the minimum required for its function.

---

## Architecture

### IPC Topology

```
┌─────────────────────────────────────────────────────────────┐
│                    IPC Topology                              │
│                                                             │
│  User Applications                                          │
│       │                                                     │
│       │ [LGP protocol — Volume III]                         │
│       ▼                                                     │
│  LGP Compositor ──── D-Bus ──────► System services         │
│       │                            (NetworkMgr, PipeWire)  │
│       │ [LGP protocol]                     │               │
│       ▼                            D-Bus ◄─┘               │
│  Shell components ── D-Bus ──────► luna-notif               │
│  (luna-shell,                                               │
│   luna-bar,                                                 │
│   luna-island)                                              │
│       │                                                     │
│       │ HTTP localhost:7734                                  │
│       ▼                                                     │
│  luna-ai-d ──── HTTP localhost:11434 ────► Ollama           │
│                                                             │
│  luna-init ─── Unix socket ──────► luna-init-ctl (CLI)     │
│  /run/luna-init.sock                                        │
│                                                             │
│  PipeWire ─── PipeWire socket ────► Audio clients          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## IPC Channels — Specification

### 1. D-Bus System Bus

**Scope:** System-level events: network state changes, hardware events, session management signals.

**Used by:**
- NetworkManager → all interested services (network state changes)
- PipeWire → audio routing events
- System services that need to publish state to multiple consumers

**Bus address:** `/run/dbus/system_bus_socket`

**Mahina D-Bus names:**

| Well-known name | Owner | Purpose |
|---|---|---|
| `org.lunakitsune.LunaInit` | luna-init | Service status signals |
| `org.lunakitsune.LunaShell` | luna-shell | Desktop shell events |
| `org.lunakitsune.LunaAI` | luna-ai-d | AI state change signals |
| `org.lunakitsune.LunaIsland` | luna-island | Luna Island state signals |
| `org.lunakitsune.LunaNotif` | luna-notif | Notification service |

```
TODO:
Decision not yet finalized.
Reason: Mahina D-Bus interface definitions (object paths, methods, signals, properties)
have not been written. The well-known names above are directional.
Full D-Bus interface XML specifications must be written before any D-Bus client
implementation begins. These belong in Volume V / 04_apis.md.
```

**Design rule:** D-Bus is for broadcast/pub-sub system events. For point-to-point request-response communication, use HTTP or Unix sockets instead. D-Bus is not a general-purpose RPC mechanism in Mahina.

### 2. HTTP — luna-ai-d API (localhost:7734)

**Scope:** All communication between shell components and the LUNA.AI daemon.

**Port:** 7734 (DL-010). Bound to `127.0.0.1` only.

**Protocol:** HTTP/1.1. JSON request and response bodies. All requests and responses include `Content-Type: application/json`.

**Consumers:** luna-shell, luna-bar, luna-island, luna-notif, CLI tools (`luna` command)

**Authentication:** None. localhost only — network-level isolation is the security boundary. See `08_security.md`.

**luna-ai-d API — Defined Endpoints:**

```
GET  /status
     → {"status": "ok"|"degraded", "mode": "DEVSHELL"|"FOCUS"|..., "version": "0.1.0"}

GET  /mode
     → {"mode": "AMBIENT", "since_ms": 45231}

POST /query
     Body: {"prompt": "...", "context": {...}, "max_tokens": 200}
     → {"response": "...", "confidence": 0.85, "mode": "CONVERSATION"}

GET  /pattern/list
     → [{"app": "code", "pattern": "build_run", "confidence": 0.91}, ...]

POST /pattern/dismiss
     Body: {"app": "code", "pattern": "build_run"}
     → {"status": "dismissed", "strike_count": 1}

POST /observe/pause
     → {"status": "paused"}     (luna observe --off)

POST /observe/resume
     → {"status": "resumed"}

DELETE /memory
     → {"status": "cleared"}    (luna memory --clear)

GET  /memory/audit
     → {full audit log as JSON}
```

```
TODO:
Decision not yet finalized.
Reason: The endpoint list above is a first-pass design, not a finalized API.
Full luna-ai-d API specification belongs in Volume IV / 03_context_engine.md
and Volume V / 04_apis.md.
Error response format, authentication (if any), rate limiting, and
versioning strategy are all unspecified.
```

**Rate limiting:** luna-ai-d imposes no rate limit in v1. Shell components are expected to be reasonable consumers. If rate limiting becomes necessary, it will be added as middleware with a Decision Log entry.

### 3. HTTP — Ollama API (localhost:11434)

**Scope:** `luna-ai-d` → Ollama communication for local model inference.

**Port:** 11434 (Ollama default). Bound to `127.0.0.1` only.

**Only `luna-ai-d` communicates with Ollama.** No shell component, no user application, and no other Mahina service makes direct calls to the Ollama API.

**Endpoints used by luna-ai-d:**

```
GET  /api/tags
     → List available models

POST /api/generate
     Body: {"model": "phi3:mini", "prompt": "...", "stream": false}
     → {"response": "...", "done": true, ...}

POST /api/chat
     Body: {"model": "phi3:mini", "messages": [...], "stream": false}
     → {"message": {"role": "assistant", "content": "..."}, "done": true}
```

`luna-ai-d` is the only component that knows the Ollama port or API format. If the Ollama API changes, only `luna-ai-d` requires updating.

### 4. Unix Domain Socket — luna-init-ctl

**Scope:** Runtime control of `luna-init` from the `luna-init-ctl` CLI tool.

**Socket path:** `/run/luna-init.sock`

**Protocol:** Newline-delimited JSON request/response.

**Access control:** Socket permissions `0600`, owned by root. Only root can connect. `luna-init-ctl` requires root or appropriate capability.

```
TODO:
Decision not yet finalized.
Reason: Whether luna-init-ctl requires root, or whether specific users/groups
can be granted access to specific commands (e.g., any user can run `status`,
only root can run `stop`), is undecided.
This is a security decision that must be recorded in the Decision Log before
luna-init-ctl is implemented.
```

Full protocol documented in `04_init_system.md`.

### 5. PipeWire Socket

**Scope:** Audio and video routing for all Mahina processes.

**Socket path:** `$XDG_RUNTIME_DIR/pipewire-0` (typically `/run/user/1000/pipewire-0`)

**Protocol:** PipeWire native protocol. Not documented here — see PipeWire upstream documentation. Mahina does not modify the PipeWire protocol.

**Used by:** Any application requiring audio I/O. luna-ai-d if voice output is enabled. luna-island if voice module is active.

WirePlumber serves as the PipeWire session manager, handling routing policy. Mahina does not write a custom session manager.

### 6. LGP Protocol — Compositor IPC

**Scope:** Communication between the LGP compositor and shell components (luna-shell, luna-bar, luna-island), and between the compositor and user applications.

```
TODO:
Decision not yet finalized.
Reason: The LGP protocol specification has not been written.
This is the highest-priority unresolved architectural decision for Volume III.
Until Volume III / 01_lgp.md is written, LGP IPC cannot be specified here.
Placeholder: The compositor communicates with shell components through some
LGP-defined protocol. The format, socket type, event model, and buffer
sharing mechanism are all TODO.
```

**What is decided:**
- LGP is a custom protocol. No Wayland protocol is used.
- The compositor and shell components are separate processes communicating through IPC.
- Shared memory buffer passing (for frame data) is likely part of LGP.

**What is not decided:**
- Socket type (Unix domain socket, shared memory, custom)
- Message format
- Event model (push vs. pull, polling vs. select)
- Buffer management protocol

---

## Technical Details

### Port Registry

All localhost ports used by Mahina services:

| Port | Service | Protocol | External |
|---|---|---|---|
| 7734 | luna-ai-d | HTTP/1.1 JSON | ❌ localhost only |
| 11434 | Ollama | HTTP/1.1 JSON | ❌ localhost only |

No other Mahina service opens a TCP or UDP port. All other IPC uses Unix domain sockets.

### IPC Security Boundary

The security model for localhost IPC is:

- No Mahina IPC port is reachable from external network interfaces. The firewall blocks all inbound connections to these ports from non-loopback interfaces.
- Any process running on the local machine can connect to localhost:7734 and localhost:11434. This is an acknowledged trust boundary — see `08_security.md` for the security threat model.
- Unix socket permissions enforce access control for luna-init-ctl.
- D-Bus policy files (in `/etc/dbus-1/system.d/`) control which processes can own which well-known D-Bus names.

### Adding a New IPC Channel

A new IPC channel may only be added when all of the following are true:

1. The need cannot be met by an existing documented channel.
2. A Decision Log entry records the new channel, its format, its consumers, and the reason existing channels are insufficient.
3. This document is updated with the new channel specification in the same change.
4. The security implications are addressed in `08_security.md`.

This rule implements Core Law I (Own Every Layer) and Core Law VI (Documentation Is Code) for the IPC layer specifically.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| luna-ai-d API versioning | v1 | Add `Accept: application/vnd.lunaos.ai+json;version=1` header support |
| D-Bus interface XML specifications | v1 | Full interface definitions for all Mahina D-Bus names |
| LGP protocol specification | v1 | Entire Volume III / 01_lgp.md |
| luna-init-ctl privilege model | v1 | Decide per-command access control |
| IPC monitoring tool | v1.5 | `luna-ipc-mon` — show all active IPC connections and message rates |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **LGP protocol.** The compositor IPC protocol is the largest unresolved question in this document. Must be resolved in Volume III before any compositor or shell implementation.

2. **luna-init-ctl privilege model.** Root-only vs. capability-based access to specific commands. Security decision — must be in the Decision Log.

3. **luna-ai-d API finalization.** The endpoint list is a first-pass design. Final API belongs in Volume IV / Volume V.

4. **D-Bus interface XML.** The well-known D-Bus names are defined but interface definitions (object paths, methods, signals, properties) are not. These must be written before any D-Bus client code is implemented.

5. **shell ↔ AI IPC alternative to HTTP.** HTTP over localhost is simple and debuggable but has higher latency than a Unix socket with a binary protocol. Whether this matters in practice depends on the frequency of shell → AI queries. No measurement data exists yet.

---

## AI Context

An AI agent implementing any Mahina component that requires IPC must:

1. Check this document first. If the required communication channel is already specified, use the documented mechanism and format. Do not invent a new channel.
2. If a new channel is required, do not implement it until a Decision Log entry is written. Flag it as a TODO instead.
3. Respect the isolation rules: no component other than `luna-ai-d` connects to Ollama. No component other than shell components connects to luna-ai-d. No Mahina service opens external network ports.
4. For LGP protocol: mark all compositor↔shell communication as TODO pending Volume III / 01_lgp.md. Do not invent a Wayland or X11 substitute.
5. All HTTP APIs use JSON. All Unix socket APIs use newline-delimited JSON. No other wire format is used in v1.
6. `luna-ai-d` listens on port 7734. Ollama listens on port 11434. These are the only two TCP ports in Mahina. If implementing something that requires a new port, it requires a Decision Log entry.

---

*Document: `Volume II / 07_ipc.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: architecture_overview.md, init_system.md, core_laws.md (Law I, VI), decision_log.md (DL-008, DL-010), non_negotiables.md*
