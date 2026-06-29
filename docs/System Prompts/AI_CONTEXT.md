# AI_CONTEXT.md — Mahina OS
**Read this file before writing any code in this repository.**
**Generated:** 2026-06-28
**Optimized for:** Claude Code, ChatGPT/Codex, and future AI coding systems.

---

## CRITICAL RULES (DO NOT SKIP)

Before reading anything else, internalize these rules:

1. **Documentation is the source of truth.** If documentation and code conflict, flag it. Never silently resolve it by choosing either side.
2. **Never invent architecture.** If a design choice requires a structural decision (new IPC mechanism, new layer, new process), stop and consult `Volume I / decision_log.md`. If no DL entry exists, prompt the human engineer to create one before writing code.
3. **Never change accepted architecture.** DL entries marked `ACCEPTED` are final. Do not contradict them or work around them.
4. **Never add dependencies without a DL entry.** Adding a new C library, system binary, or Python package requires a Decision Log entry first.
5. **Consult `Volume II / 13_component_ownership.md` before touching IPC.** Every IPC interface has a designated owner process. Do not route communications through the wrong process.
6. **Code that violates `Volume VI / 01_coding_standards.md` will not be merged.** Read that document before writing any C.

---

## Project Vision

Mahina is a documentation-first, ground-up Linux operating system. It is built to be:
- **Deterministic:** Every boot is identical. Every service start order is defined.
- **Owned:** Every component is written by the project or deeply understood. No mystery layers.
- **Alive:** The system has a digital AI presence (LUNA) that inhabits it, not runs on it.
- **Aesthetic:** Every visual element communicates system state. Motion is never decorative.

The OS is named **Mahina**. The AI digital presence within it is named **LUNA**. These are distinct.

---

## Core Philosophy (Non-Negotiable)

**Three Laws (from `Volume I / philosophy.md`):**

| Law | Principle |
|---|---|
| I — Own Every Layer | Every component written from scratch or deeply understood. No distro base. |
| II — Local First | Fully offline operation. AI works offline. Cloud is opt-in. |
| III — Aesthetic Is Functional | Motion = system state. Never decoration. |

**Non-Negotiables (from `Volume I / non_negotiables.md`):**
- ❌ Wayland, Hyprland, GNOME, KDE — NONE of these ship.
- ✅ LGP (Luna Graphics Protocol) is the ONLY graphics layer.
- ✅ Local-first AI. No mandatory cloud.
- ✅ Rolling release.
- ✅ LUNA is a digital presence, not an application.
- ✅ Motion communicates state.

---

## Canonical Terminology

| Term | Meaning |
|---|---|
| `luna-init` | The PID 1 init system (C17, statically linked) |
| `luna-splash` | Boot graphics engine (writes to /dev/fb0) |
| `luna-init-ctl` | CLI control tool for luna-init's Unix socket |
| `LGP` | Luna Graphics Protocol — Mahina's custom display protocol |
| `lgp-compositor` | The process that implements LGP — owns the display |
| `LunaGUI` | The widget toolkit that applications use to draw windows |
| `luna-shell` | Primary desktop shell (future Stage 6) |
| `luna-island` | LUNA's ambient presence indicator in the dock (future) |
| `luna-bar` | Status bar (future) |
| `luna-ai-d` | The LUNA AI daemon (Python asyncio, future Stage 7) |
| `luna-lock` | Screen lock daemon (future Stage 3) |
| `lpkg` | Mahina package manager (Python v1, future) |
| `DCKL` | Document Control Knowledge Library — the docs in `docs/DCKL/` |
| `DL-NNN` | Decision Log entry in `Volume I / decision_log.md` |
| `AR-NNN` | Architecture Review session in `docs/Architecture Reviews/` |
| `g_services` | Global service table in luna-init (max 32 services) |
| `LUNA_STAGE_*` | Boot stage constants (0–7) used in log formatting |
| `READY_FILE/SOCKET/HTTP/SIGNAL` | Service readiness detection methods |

---

## Architecture Overview

### Current System (Stage 0, v0.1)

```
Bootloader: limine
     │
     ▼
Kernel: Linux 6.6.x LTS (x86_64, kernel/.config fragment)
     │
     ▼
PID 1: luna-init (statically linked C17 binary)
     │
     ├── Early mounts: /proc /sys /dev (critical, inline before log)
     ├── fstab.toml mounts: /dev/pts /tmp /run /dev/shm
     ├── hostname from /etc/luna/hostname
     ├── Entropy seed from /etc/machine-id
     │
     ├── Stage 4: Load /etc/luna/services/*.toml
     │           Build dep graph (Kahn's), detect cycles
     │           Start services in topological order
     │
     ├── Control socket: /run/luna-init.sock (JSON protocol)
     │
     └── Event loop: epoll(signalfd, ctl_fd)
           SIGCHLD → reaper → supervisor restart logic
           SIGTERM → shutdown
           SIGINT  → reboot
           SIGHUP  → reload service definitions
           SIGUSR1 → dump state to log

PID N: luna-splash (child of luna-init)
     │  Reads: /dev/fb0 (mmap)
     │  Receives: pipe IPC "PERCENT|MSG\n" from luna-init
     └── Renders: logo + progress text + progress bar
```

### Current Graphics System (Stage 5 bring-up)

Stage 5 is partially implemented. `luna-init` releases `luna-splash` and starts
`lgp-compositor` after the splash fade-out. `lgp-compositor` has real M1-M3 code:
DRM/KMS dumb-buffer setup, `/run/lgp/compositor.sock`, 6-byte TLV framing per
DL-053, `LGP_HELLO`, persistent client lifecycle state, stream reassembly,
basic `LGP_CREATE_SURFACE` / `LGP_DESTROY_SURFACE` / `LGP_COMMIT_BUFFER`, and
shared-memory `XRGB8888` commits via `SCM_RIGHTS`.

Still not implemented: LunaGUI, luna-shell, luna-island, luna-bar, luna-ai-d,
lpkg, and luna-lock. For planned architecture, read the relevant DCKL volume.

---

## Folder Structure

```
mahina-os/
├── src/luna-init/         C17 source — all PID 1 logic (20 .c/.h files)
├── src/luna-init-ctl/     C17 source — control CLI (main.c only)
├── src/luna-splash/       C17 source — framebuffer boot splash (4 files)
├── tests/unit/luna-init/  Active unit tests (4 files, 1 test each)
├── tests/unit/            STALE test files — NOT in TEST_SOURCES; do not edit
├── tests/fuzz/toml/       AFL++ harness for TOML parser
├── tests/vendor/unity/    Unity test framework (MIT)
├── kernel/                .config fragment + .config.notes
├── boot/                  limine.conf
├── etc/luna/              Runtime config: fstab.toml, hostname, services/
├── scripts/               build-kernel, build-initramfs, build-image, run-qemu
├── docs/DCKL/             7-volume architecture library
└── docs/Architecture Reviews/  AR session records + sprint reports
```

---

## Coding Standards Summary

**Full reference: `docs/DCKL/Volume VI - Development Bible/01_coding_standards.md`**

### Language Rules

- **C17** for all systems code (luna-init, lgp-compositor, LunaGUI, luna-splash)
- **C++ is forbidden** in the base system
- **Python** for luna-ai-d (v1) and lpkg (v1)
- **No Rust in v1.** Rust v2 migration planned for luna-ai-d and lpkg only.

### Compiler Flags (non-negotiable)

```
-std=c17 -Wall -Wextra -Werror -Wpedantic -Wstrict-prototypes
-Wmissing-prototypes -Wcast-align -Wformat=2 -Wnull-dereference
-O2 -fstack-protector-strong
```

**Zero-warning policy. A warning is a build failure.**

### Naming Conventions

```c
typedef struct { ... } snake_case_t;               // structs
void namespace_verb_noun(void);                    // functions
int local_variable;                                // variables
#define UPPER_SNAKE_CASE 42                        // macros/constants
typedef enum { NAMESPACE_VALUE } namespace_enum_t; // enums
```

### Memory Rules

- Hot paths (compositor render loop, LGP message parser): **NO malloc/free**
- All structs zero-initialized: `calloc(1, sizeof(T))` or `= {0}`
- Every `malloc()` has a documented owner
- Functions returning allocated memory include `alloc`/`create`/`dup` in their name

### Error Handling

- Functions that can fail return `int` (0 = success, negative = error) or nullable pointer
- **No silent failures.** Every error is logged or propagated.
- Use early returns; `goto cleanup` is permitted for resource cleanup on error paths.

### Header Rules

- Every `.c` has a corresponding `.h`
- Headers use `#ifndef MAHINA_MODULE_H` guards
- Only public API symbols appear in headers; internal symbols use `static`

### Testing Requirements

- Unit tests use the Unity framework (`tests/vendor/unity/`)
- Tests compile with `-fsanitize=address,undefined -fno-sanitize-recover=all`
- Run: `make test-unit`
- Fuzz: `make test-fuzz` (TOML parser via AFL++)
- Lint: `make lint` (clang-tidy, zero warnings)

---

## Decision Log Summary

**Full log: `docs/DCKL/Volume I - Foundation/decision_log.md`**
**51 accepted entries (DL-001 through DL-051) plus 2 Architectural Principles.**

Key decisions relevant to coding:

| DL | Decision | Coding Impact |
|---|---|---|
| DL-002 | luna-init in C17 | Static binary, no external deps, no C++ |
| DL-004R | LGP compositor replaces Wayland/Hyprland | Never link against wayland-client or hyprland IPC |
| DL-008 | TOML config format | All config in .toml; never YAML, JSON, INI |
| DL-021 | AI = Presence Engine + LLM (lazy) | Ollama NOT started at boot; only on first AI demand |
| DL-053 | LGP wire: TLV (2B type, 4B len, N bytes payload) | All LGP codec must use this exact 6-byte framing |
| DL-026 | GPU: software renderer Stage 2, Vulkan Stage 3 | No GPU API calls in Stage 2 compositor code |
| DL-027 | Btrfs root | Installer + lpkg must use btrfs snapshot before updates |
| DL-031 | Compositor ready signal = D-Bus `org.mahina.compositor.Ready` | Stage 6 services block on this D-Bus signal |
| DL-032 | Input = libinput | lgp-compositor uses libinput; nothing else opens /dev/input/* |
| DL-033 | Clipboard = LGP extension `lgp_ext_clipboard_v1` | No D-Bus clipboard service; no app-to-app clipboard |
| DL-035 | Screen lock = `luna-lock` process | luna-init and luna-shell do NOT lock the screen |
| DL-042 | AI memory = dynamic (Presence Engine < 200 MB, LLM = total_ram/4) | No hardcoded 6 GB cap |
| DL-048 | lpkg signing = Ed25519 via libsodium | Never GPG; never RSA |
| DL-049 | luna-ai-d = Python asyncio v1 | No blocking calls on main event loop |

---

## Architecture Review Summary

**Full records: `docs/Architecture Reviews/`**

| Session | Key Outcomes |
|---|---|
| AR-001 | Initial architecture validation |
| AR-002 | Hybrid graphics model (DL-004R), filesystem, networking, AI split |
| AR-004 | LGP wire format (DL-025), GPU stages (DL-026), Btrfs (DL-027), Bitcount font (DL-028), libinput (DL-032), luna-lock (DL-035), wpa_supplicant (DL-036) |
| AR-006 | Default LLM Qwen2.5 3B (DL-046), offline behavior (DL-047), Ed25519 signing (DL-048), Python luna-ai-d (DL-049), Inter reading font (DL-050), Phosphor Icons (DL-051) |

---

## Current Implementation Status

### Implemented (complete, write code that calls these)

| Module | File | What It Provides |
|---|---|---|
| Logger | `src/luna-init/log.c` | `LUNA_INFO/WARN/ERROR/FATAL()` macros; `luna_log_init()`, `luna_log_close()` |
| TOML parser | `src/luna-init/toml.c` | `toml_load_file()`, `toml_load_buffer()`, `toml_get()`, `toml_free()` |
| Service parser | `src/luna-init/service.c` | `service_load_all()`, `service_find()`, `service_find_by_pid()` |
| Dep graph | `src/luna-init/depgraph.c` | `depgraph_build()`, `depgraph_topo_sort()` |
| Supervisor | `src/luna-init/supervisor.c` | `supervisor_start_all()`, `supervisor_start_one()`, `supervisor_stop_one()` |
| Mount manager | `src/luna-init/mount.c` | `mount_early()`, `mount_fstab()`, `mount_unmount_all()` |
| Signal handler | `src/luna-init/signal.c` | `signal_init()` returns signalfd; `signal_read()` returns `signal_action_t` |
| Zombie reaper | `src/luna-init/reaper.c` | `reaper_reap_all()` |
| Shutdown | `src/luna-init/shutdown.c` | `shutdown_run(SHUTDOWN_POWEROFF/REBOOT)` — never returns |
| Panic | `src/luna-init/panic.c` | `panic_drop_to_shell(reason)` — never returns |
| Console | `src/luna-init/console.c` | `console_print_welcome()`, `console_drop_to_shell()` |
| Hostname | `src/luna-init/hostname.c` | `hostname_set(path)` |
| Splash bridge | `src/luna-init/splash.c` | `splash_start()`, `splash_update(msg, pct)`, `splash_stop()` |
| CTL server | `src/luna-init/ctl.c` | `ctl_server_init()`, `ctl_server_accept()`, `ctl_server_close()` |
| FB renderer | `src/luna-splash/render.c` | `render_init()`, `render_clear()`, `render_text()`, `render_logo()`, `render_progress()` |
| Splash IPC | `src/luna-splash/ipc.c` | `ipc_init(fd)`, `ipc_read_event()`, `ipc_cleanup()` |

### Partially Implemented (Stage 5)

`src/lgp-compositor/` exists and builds. It currently covers the early compositor
bring-up path: DRM/KMS initialization, socket lifecycle, TLV framing, `LGP_HELLO`,
client identity via `SO_PEERCRED`, persistent per-connection state, and bounded
receive-buffer parsing for split stream reads.

### Not Implemented (Stage 6+)

These subsystems still have zero code: `LunaGUI`, `luna-shell`, `luna-island`,
`luna-bar`, `luna-ai-d`, `lpkg`, and `luna-lock`.

---

## Things AI Must NEVER Change

1. **The IPC protocol of luna-splash** (`PERCENT|MSG\n` pipe format). luna-init and luna-splash must agree.
2. **The CTL socket JSON protocol** in `ctl.c`. luna-init-ctl depends on this exact format.
3. **The TOML parser public API** in `toml.h`. Any change breaks the fuzz corpus and all callers.
4. **Service file format** (the `.toml` schema in `etc/luna/services/`). luna-init reads these at boot.
5. **Signal handling architecture** in `signal.c`. signalfd + epoll is non-negotiable (race-free, per DL-002).
6. **Boot stage sequence** (Stages 0–7 in `main.c`). This is the Mahina boot contract.
7. **Accepted Decision Log entries (DL-*).** These are immutable unless a new superseding DL is created.
8. **The non-negotiables** in `Volume I / non_negotiables.md`. Not subject to workarounds.
9. **`g_services[SERVICE_MAX_COUNT]` table structure** — the global service table is shared across all luna-init modules.
10. **luna-init static linking** — PID 1 MUST be statically linked. Never add dynamic library dependencies.

---

## Current Sprint / Current Priorities

**Current state:** Stage 5 graphics bring-up is in progress. The system boots
through `luna-splash`, releases it, starts `lgp-compositor`, and falls back to
interactive shells while Stage 6 clients are still absent.

**Next milestone (Phase 2 continuation):**
- Harden luna-init audit findings that affect Stage 5 supervision.
- Continue `lgp-compositor` toward documented surface creation and buffer commit.
- Keep docs synchronized when implementation state changes.

---

## Open TODOs in Code

These are documented stubs — they are KNOWN and tracked, not accidents:

| File | Issue | Status |
|---|---|---|
| `supervisor.c:READY_HTTP` | HTTP readiness check stubbed — returns true immediately | Planned for v0.5 |
| `supervisor.c:READY_SIGNAL` | SIGUSR1 readiness stubbed — returns true immediately | Planned for v0.5 |
| `supervisor.c` | `run_user`/`run_group` parsed but setuid/setgid never called | Security gap for production |
| `luna_log_switch_to_runtime()` | Never called in main.c — runtime log permanently unused | Low priority |
| `render.c:render_logo()` | Block character rendering loop is empty; falls back to ASCII "MAHINA" | Phase 1 finish |
| `main.c` stages 5–7 | Intentionally empty — Stages 5/6/7 not implemented until v0.5/v0.9 | By design |

---

## Cross-References to Detailed Documents

| Topic | Document |
|---|---|
| Philosophy & Laws | `docs/DCKL/Volume I - Foundation/philosophy.md` |
| All decisions | `docs/DCKL/Volume I - Foundation/decision_log.md` |
| Non-negotiables | `docs/DCKL/Volume I - Foundation/non_negotiables.md` |
| Glossary | `docs/DCKL/Volume I - Foundation/glossary.md` |
| Boot sequence | `docs/DCKL/Volume II - Architecture/02_boot_flow.md` |
| linux-init spec | `docs/DCKL/Volume II - Architecture/04_init_system.md` |
| Component ownership | `docs/DCKL/Volume II - Architecture/13_component_ownership.md` |
| LGP protocol | `docs/DCKL/Volume III - Graphics & Presence/01_lgp.md` |
| Compositor | `docs/DCKL/Volume III - Graphics & Presence/03_compositor.md` |
| LunaGUI | `docs/DCKL/Volume III - Graphics & Presence/04_lunagui.md` |
| AI runtime | `docs/DCKL/Volume IV - AI & Presence/00_luna_runtime.md` |
| Package manager | `docs/DCKL/Volume V - Userland & SDK/03_package_manager.md` |
| Coding standards | `docs/DCKL/Volume VI - Development Bible/01_coding_standards.md` |
| AI coding rules | `docs/DCKL/Volume VI - Development Bible/02_ai_coding_guidelines.md` |
| Architecture rules | `docs/DCKL/Volume VI - Development Bible/03_architecture_rules.md` |
| Benchmarks | `docs/DCKL/Volume VI - Development Bible/04_benchmarks.md` |
| Testing standards | `docs/DCKL/Volume VI - Development Bible/05_testing_standards.md` |
| Milestones | `docs/DCKL/Volume VI - Development Bible/06_milestones.md` |
| Implementation roadmap | `docs/DCKL/Volume VII - Implementation Roadmap/01_implementation_roadmap.md` |

**For fast AI loading:** `Mahina_DCKL_Complete.md` at the repo root is a 23,826-line monolithic concatenation of all DCKL documents. Load it if you need full architecture context in a single pass.
