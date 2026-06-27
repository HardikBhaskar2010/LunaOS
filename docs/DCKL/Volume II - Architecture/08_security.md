# Mahina — Security Architecture
**Volume II · Chapter 8**
**Classification:** Core Architecture — Security
**Status:** Active · Reference for all component implementation

---

## Purpose

This document specifies the Mahina security architecture: the threat model, privilege separation model, mandatory access control policy, IPC security boundaries, and user-facing security guarantees.

Security in Mahina is not a feature layer applied on top of a working system. It is a structural property of the system from the kernel upward. Every component's security posture is defined before that component is implemented.

---

## Overview

Mahina security is organized around five axes:

1. **Privilege separation** — Processes run with the minimum privilege necessary
2. **Mandatory access control (MAC)** — AppArmor profiles constrain what processes can do
3. **IPC isolation** — All inter-process communication is bounded to documented channels
4. **User data sovereignty** — `~/.luna/` is exclusively the user's (Core Law II)
5. **No telemetry, ever** — Core Law V: no data leaves the machine without explicit user action

---

## Design Philosophy

Security decisions in Mahina follow three rules derived from the Core Laws:

**Rule 1 — Least privilege (from Law I).** Every process runs with the minimum set of capabilities and file permissions required to perform its function. A process that needs to read `/etc/luna/services/` does not get write access to `/`.

**Rule 2 — No hidden outbound traffic (from Law II, Law V).** Mahina never transmits data without explicit user instruction. No crash reports. No telemetry. No analytics. No "improve Mahina" data collection. The only outbound network traffic initiated by Mahina infrastructure is:
- `lpkg` fetching packages when the user runs `lpkg update` or `lpkg install`
- Ollama pulling model weights when the user runs `ollama pull`
- Cloud bridge calls when the user explicitly invokes `luna bridge --send`

Anything else is a bug.

**Rule 3 — Security by architecture, not security by policy alone.** AppArmor profiles and filesystem permissions enforce constraints. They do not paper over design flaws. If `luna-ai-d` does not need root, it does not run as root — and no AppArmor profile is needed to prevent it from doing root things.

---

## Threat Model

### In Scope

| Threat | Mitigation |
|---|---|
| Malicious application reading `~/.luna/` (user data theft) | Filesystem permissions, AppArmor profile for `luna-ai-d` |
| Malicious application connecting to `localhost:7734` (AI query injection) | localhost trust boundary documented; see Out of Scope below |
| Privilege escalation via `lpkg` (package manager installing rootkits) | Package signing, `lpkg` does not run as root directly |
| `luna-ai-d` sending data to external servers without permission | `luna-ai-d` never makes outbound calls except cloud bridge (opt-in) |
| Process crash in one cgroup slice affecting another | cgroup memory.max limits, process isolation |
| Malformed service file causing luna-init to execute arbitrary code | Service file validation before execution |

### Out of Scope (v1)

| Threat | Reason Out of Scope |
|---|---|
| Malicious local process connecting to localhost:7734 | Any local process can connect to localhost. This is a v1 known limitation. A capability-based auth token is a v2 improvement. |
| Physical hardware attacks (cold boot, DMA attacks) | v1 does not implement hardware security measures |
| Side-channel attacks (Spectre/Meltdown mitigations) | Standard kernel mitigations enabled by default; no additional Mahina-specific work |
| Kernel exploits | Kernel hardening is a v2 concern; KASLR is the primary mitigation in v1 |
| Multi-user security | Mahina v1 is a single-user system |

---

## Architecture

### Privilege Hierarchy

```
┌─────────────────────────────────────┐
│            root (uid 0)              │
│  luna-init (PID 1)                  │
│  lpkg (when installing packages)    │
│  Hardware/device management         │
├─────────────────────────────────────┤
│            luna (dedicated uid)      │
│  luna-ai-d                          │
│  ollama                             │
├─────────────────────────────────────┤
│            user (runtime uid)        │
│  LGP compositor                     │
│  luna-shell, luna-bar               │
│  luna-island, luna-notif            │
│  User applications                  │
└─────────────────────────────────────┘
```

**luna-init (root):** Must run as root to mount filesystems, create device nodes, and set up cgroup hierarchies. After Stage 3 early hooks, luna-init drops all capabilities it no longer needs before starting services.

```
TODO:
Decision not yet finalized.
Reason: The specific Linux capabilities retained by luna-init after Stage 3
have not been enumerated. A capability audit of luna-init's post-boot needs
is required before implementation.
At minimum: CAP_SYS_ADMIN (cgroups), CAP_SETUID/SETGID (service user switching),
CAP_KILL (service management). All others should be dropped.
```

**luna (dedicated system user):** A non-login, no-shell user with uid/gid assigned at install time. `luna-ai-d` and `ollama` run as this user. They have read/write access to `~/.luna/` (owned by the current logged-in user, readable by `luna` via group or ACL) and no other elevated access.

```
TODO:
Decision not yet finalized.
Reason: The exact permission model for luna-ai-d accessing ~/.luna/ (owned by user)
has not been decided.
Option A: ~/.luna/ is world-readable but luna-ai-d AppArmor profile restricts
          all other processes from reading it.
Option B: The `luna` group is added to the logged-in user's groups, and
          ~/.luna/ is group-readable by `luna`.
Option C: luna-ai-d runs as the logged-in user (not a dedicated `luna` user).
Option C is simplest but means luna-ai-d has all user permissions, not just
access to ~/.luna/. Security tradeoff must be recorded as a Decision Log entry.
```

**User (runtime uid):** The logged-in user. The LGP compositor and all shell components run as this user. They have access to the user's session resources (display, audio via PipeWire, files in the user's home directory).

### Filesystem Permissions

Key permission boundaries:

| Path | Owner | Mode | Purpose |
|---|---|---|---|
| `/etc/luna/` | root | `755` | System configuration — readable, root-writable |
| `/etc/luna/services/` | root | `755` | Service files — readable, root-writable |
| `~/.luna/` | user | `700` | LUNA.AI data — user only |
| `~/.luna/memory/` | user | `700` | AI memory store — user only |
| `~/.luna/config/observe.toml` | user | `600` | Observation config — user only |
| `/run/luna-init.sock` | root | `600` | Init control socket — root only |
| `/var/log/luna-init/` | root | `755` | Boot and runtime logs |

### AppArmor Profiles

Mahina uses AppArmor as the mandatory access control system (`CONFIG_DEFAULT_SECURITY_APPARMOR=y` — see `03_linux_architecture.md`).

AppArmor profiles are stored in `/etc/apparmor.d/` and are activated by luna-init at boot.

**Required profiles for v1:**

| Profile | Process | Key restrictions |
|---|---|---|
| `luna-ai-d` | `luna-ai-d` | Read/write `~/.luna/`; connect to localhost:11434; connect to localhost:7734 (own port); deny all filesystem writes outside `~/.luna/` and `/var/log/luna-ai-d.log`; deny all outbound network except localhost and cloud bridge whitelist |
| `ollama` | `ollama` | Read model weight files from Ollama model dir; connect localhost:11434; deny all outbound network |
| `lgp-compositor` | LGP compositor | Access DRM device `/dev/dri/card*`; access framebuffer; deny network |
| `luna-shell` | `luna-shell` | Access LGP compositor socket; read user files; deny root filesystem writes |
| `lpkg` | `lpkg` | Read/write `/var/lib/lpkg/`; read package cache; network access (package fetch only); deny `~/.luna/` |

```
TODO:
Decision not yet finalized.
Reason: AppArmor profile text for each component has not been written.
Profile writing requires knowing the exact filesystem access patterns of each
component, which in turn requires the component to be implemented or prototyped.
Profile writing is a v1 pre-release task, not a design task.
This section will be updated with the actual profile syntax once components exist.
```

### Kernel Hardening

Enabled in v1 (from `03_linux_architecture.md`):

| Hardening measure | Mechanism | Status |
|---|---|---|
| KASLR | `CONFIG_RANDOMIZE_BASE=y` | ✅ Enabled |
| User-space ASLR | `randomize_va_space = 2` | ✅ Enabled |
| Kernel pointer hiding | `kptr_restrict = 2` | ✅ Enabled |
| Non-executable kernel data | `CONFIG_STRICT_KERNEL_RWX=y` | ✅ Enabled |
| dmesg restriction | `dmesg_restrict = 1` | ✅ Enabled |
| Seccomp filter support | `CONFIG_SECCOMP_FILTER=y` | ✅ Available — not yet applied to all services |
| Stack canaries | gcc `-fstack-protector-strong` | ✅ Build flag |

Not enabled in v1:

| Hardening measure | Reason Deferred |
|---|---|
| Secure Boot | limine signing infrastructure not scoped for v1 |
| Measured boot (TPM) | TPM integration is v2 complexity |
| Full seccomp profiles per service | Per-service syscall filtering requires profiling each service — v1.5 task |
| Kernel lockdown mode | Incompatible with some v1 debugging requirements |

### IPC Security Boundaries

All IPC security is documented in detail in `07_ipc.md`. Summary of security posture:

- **localhost:7734 (luna-ai-d):** Any local process can connect. v1 known limitation. Mitigation: `luna-ai-d` validates that requests do not request actions outside its defined API.
- **localhost:11434 (Ollama):** Any local process can connect. Mitigation: Only `luna-ai-d` is expected to use this port. Documented trust assumption.
- **`/run/luna-init.sock`:** Permissions `0600`, root only. Hard access control.
- **D-Bus system bus:** D-Bus policy files (`/etc/dbus-1/system.d/`) control which processes can own Mahina well-known names.

### Firewall Configuration

The default Mahina firewall rules (implemented via nftables, loaded by a luna-init service at Stage 4):

```
# /etc/luna/nftables.conf
table inet luna_firewall {
    chain input {
        type filter hook input priority 0; policy drop;

        # Accept established connections
        ct state established,related accept

        # Accept loopback
        iif "lo" accept

        # Accept ICMP (ping)
        ip protocol icmp accept
        ip6 nexthdr icmpv6 accept

        # SSH (if enabled by user — off by default)
        # tcp dport 22 accept

        # Drop everything else
        drop
    }

    chain output {
        type filter hook output priority 0; policy accept;
        # All outbound traffic accepted by default
        # luna-ai-d's AppArmor profile restricts outbound at the application level
    }

    chain forward {
        type filter hook forward priority 0; policy drop;
    }
}
```

The firewall:
- Drops all inbound traffic not matching an established connection or loopback
- Allows all outbound traffic (AppArmor profiles restrict per-application network access)
- Has no default port opens — the user opens ports explicitly if needed

---

## Technical Details

### lpkg Security Model

`lpkg` is the Mahina package manager. It installs software that may require root filesystem access. The security model:

```
TODO:
Decision not yet finalized.
Reason: The privilege escalation model for lpkg has not been specified.
Options:
  Option A: lpkg itself runs as root (simple, but means the Python package manager
            runs as root for all operations including dependency resolution).
  Option B: lpkg uses a minimal setuid helper binary (written in C) only for the
            final install step (file copy + chmod). Dependency resolution runs unprivileged.
  Option C: lpkg uses pkexec or a PolicyKit-equivalent for privilege escalation.
Recommendation: Option B — a minimal setuid C helper is consistent with Law I
(the helper is small enough to fully understand and audit).
This must be a Decision Log entry before lpkg implementation.
```

### Package Signing

All Mahina packages distributed through the official repository must be cryptographically signed. `lpkg` verifies package signatures before installation.

```
TODO:
Decision not yet finalized.
Reason: Package signing algorithm and key management infrastructure have not been designed.
The following must be specified before the package repository is built:
  - Signing algorithm (Ed25519 recommended)
  - Key distribution mechanism (bundled in lpkg vs. fetched at install)
  - Key rotation policy
  - Repository signature format (detached signature file vs. signed manifest)
This is a v1 pre-release deliverable.
```

### User Data Sovereignty Guarantees

These are not aspirational. They are behavioral requirements enforced by architecture and AppArmor:

| Guarantee | Enforcement Mechanism |
|---|---|
| `~/.luna/` readable only by user and `luna-ai-d` | Filesystem permissions + AppArmor deny rules on all other processes |
| `luna memory --clear` always works | CLI command is a direct syscall sequence — no system component can block it |
| `luna memory --audit` always works | Read-only operation on user-owned files — cannot be blocked |
| No telemetry or crash reports sent | `luna-ai-d` AppArmor profile denies outbound except localhost and cloud bridge whitelist |
| Cloud bridge is opt-in only | `luna-ai-d` code path for cloud calls is only reachable via explicit user command |
| `luna observe --off` always works | Sends signal to `luna-ai-d`; `luna-ai-d` is required to honor it immediately |

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| localhost:7734 authentication token | v2 | Prevent local processes from querying luna-ai-d without a session token |
| Full seccomp profiles per service | v1.5 | Per-service syscall allowlists |
| Secure Boot support | v2 | limine signing + key management |
| Package signing infrastructure | v1 | Required before public release |
| AppArmor profiles for all services | v1 | Write and test profiles for all components |
| lpkg setuid helper | v1 | Minimal C binary for privileged install operations |
| luna-ai-d capability audit | v1 | Enumerate minimum linux capabilities for luna-ai-d |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **luna-ai-d user model.** `luna` dedicated user vs. running as the logged-in user. Security tradeoff must be in the Decision Log.

2. **lpkg privilege escalation model.** setuid helper vs. pkexec vs. full root. Must be in the Decision Log before lpkg implementation.

3. **Package signing algorithm and key distribution.** Must be designed and recorded before the package repository goes public.

4. **luna-init capability audit.** Which Linux capabilities does luna-init retain post-Stage 3? Must be enumerated before implementation.

5. **localhost:7734 authentication.** v1 accepts any local connection. A session token system is planned for v2 but not designed.

6. **Seccomp profiles.** Per-service syscall allowlists reduce the impact of RCE vulnerabilities significantly. They require profiling each service's syscall usage. Deferred to v1.5 but should begin profiling in v1.

---

## AI Context

An AI agent implementing any Mahina component must understand:

- Every process runs at the minimum privilege necessary. If a component does not need root, it must not run as root. Document the reason if root is unavoidable.
- `~/.luna/` is exclusively the user's. No process except `luna-ai-d` may open files in this directory. This is enforced by both filesystem permissions and AppArmor.
- `luna-ai-d` makes no outbound network calls except: (a) to localhost:11434 (Ollama), (b) to the cloud bridge whitelist when `luna bridge --send` is explicitly called by the user. Any other outbound connection from `luna-ai-d` is a security violation.
- The firewall default-drops all inbound connections except loopback and established state. Do not add firewall rules that open ports to external interfaces without a Decision Log entry and user instruction.
- AppArmor profiles are required for all Mahina daemons before v1 public release. If implementing a new daemon, the AppArmor profile is part of the implementation deliverable.
- Package signing is required before any packages are distributed publicly. Do not implement a package distribution mechanism without implementing signature verification in lpkg simultaneously.
- `luna memory --clear` and `luna observe --off` must always work. No code path may intercept, delay, or deny these commands.

---

*Document: `Volume II / 08_security.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: architecture_overview.md, linux_architecture.md, init_system.md, scheduler.md, memory.md, ipc.md, core_laws.md (Law II, V), non_negotiables.md, decision_log.md*
