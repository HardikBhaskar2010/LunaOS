# PROJECT_STATE.md — Mahina OS
**Generated:** 2026-06-28
**Canonical document. Update after every major milestone.**
**Audience:** New contributors, project leads, and AI coding agents needing a fast onramp.**

---

## What Is Mahina?

Mahina is a ground-up Linux-based operating system built under a strict Documentation-First methodology. Every architectural decision is recorded in the Divine Collection of Knowledge about Luna (DCKL) before code is written. The code exists solely to satisfy the documentation — never the reverse.

**Three Laws govern every decision (see `docs/DCKL/Volume I - Foundation/philosophy.md`):**
- **Law I — Own Every Layer:** Every component is written from scratch or deeply understood. No distro base. No mystery components.
- **Law II — Local First, Cloud When Useful:** The machine works fully offline. AI works fully offline. Cloud is opt-in.
- **Law III — Aesthetic Is Functional:** Every animation communicates state. Beauty is information density, not decoration.

**The system consists of:**
- A custom init system (`luna-init`) as PID 1
- A graphical boot splash (`luna-splash`) rendering to `/dev/fb0`
- A custom display protocol (`LGP` — Luna Graphics Protocol) *(planned)*
- A digital AI presence (`LUNA`) running via `luna-ai-d` and Ollama *(planned)*
- A custom package manager (`lpkg`) *(planned)*
- A custom shell environment (`luna-shell`, `luna-island`, `luna-bar`) *(planned)*

The OS is renamed from LunaOS to **Mahina** (DL-045, AR-006). LUNA remains the name of the AI presence within Mahina.

---

## Current Architecture

### Stack Summary

| Layer | Component | Status | Language |
|---|---|---|---|
| Bootloader | limine (DL-005) | Config complete | TOML |
| Kernel | Linux 6.6.x LTS (DL-009) | Config fragment committed | Kconfig |
| PID 1 | luna-init | **COMPLETE** | C17 |
| Boot splash | luna-splash | **COMPLETE** | C17 |
| Init control CLI | luna-init-ctl | **COMPLETE** | C17 |
| Display protocol | LGP compositor | NOT STARTED | C17 |
| UI toolkit | LunaGUI | NOT STARTED | C17 |
| AI daemon | luna-ai-d | NOT STARTED | Python (DL-049) |
| Shell | luna-shell / luna-island | NOT STARTED | C17 |
| Package manager | lpkg | NOT STARTED | Python v1 (DL-003) |
| Screen lock | luna-lock | NOT STARTED | C17 |

### Boot Sequence (7 stages)

```
Stage 0: Kernel handoff → luna-init alive (PID 1)
Stage 1: Signal handling, zombie reaper, epoll loop
Stage 2: Filesystem mounts (/proc /sys /dev /tmp /run /dev/shm /dev/pts)
Stage 3: Early hooks (hostname, entropy seed)
Stage 4: Service supervisor init — load and start all services
Stage 5: LGP compositor start        [NOT IMPLEMENTED — v0.5+]
Stage 6: Shell + luna-island start    [NOT IMPLEMENTED — v0.5+]
Stage 7: AI runtime ready             [NOT IMPLEMENTED — v0.9+]
```

In v0.1 (current), the system boots to Stages 0–4, then drops to an interactive root shell on TTY1. Stages 5–7 are intentionally empty stubs.

### Service Supervision Model

Services are defined as TOML files in `/etc/luna/services/*.toml`. luna-init:
1. Reads all `.toml` files at Stage 4
2. Builds a dependency graph (Kahn's algorithm) and detects cycles
3. Starts services in topological order
4. Waits for each service's readiness before proceeding (file / socket / HTTP / signal methods)
5. Restarts services on failure per policy (always / on-failure / never)
6. Supervises via SIGCHLD → signalfd → epoll event loop

Current defined services (all production-target, none run successfully in v0.1 since binaries don't exist yet):
`dbus`, `networkmanager`, `ntpd`, `pipewire`, `wireplumber`, `ollama`, `lgp-compositor`, `luna-ai-d`

### IPC Architecture

| Interface | Protocol | Path |
|---|---|---|
| luna-init control | Newline-delimited JSON over Unix socket | `/run/luna-init.sock` |
| D-Bus system bus | D-Bus | `/run/dbus/system_bus_socket` |
| LGP compositor | TLV binary (DL-025) | `/run/lgp/compositor.sock` |
| luna-ai-d | Internal D-Bus (DL-031) | TBD |
| luna-splash IPC | Pipe: `PERCENT|MSG\n` text | Pipe FD passed via `--fd` arg |

### Key Architectural Decisions (summary of 51 DL entries)

| ID | Topic | Decision |
|---|---|---|
| DL-001 | Base distro | Linux From Scratch — no upstream base |
| DL-002 | Init system | luna-init in C17 |
| DL-003 | Package manager | lpkg in Python v1 |
| DL-004R | Graphics | LGP compositor + LunaGUI hybrid (Hyprland rejected) |
| DL-005 | Bootloader | limine |
| DL-006 | AI runtime | Ollama → superseded by DL-046 |
| DL-007 | C library | glibc v1, musl v2 |
| DL-008 | Config format | TOML for all config |
| DL-009 | Kernel | Linux 6.6.x LTS |
| DL-025 | LGP wire format | TLV binary (1B type + 4B length + payload) |
| DL-026 | GPU backend | Software renderer Stage 2, Vulkan Stage 3 |
| DL-027 | Root filesystem | Btrfs (snapshots before updates) |
| DL-028 | Display typeface | Bitcount (personality) + Inter (reading) |
| DL-034 | Luna Island | Short click = compact panel, long press = full conversation |
| DL-039 | Themes | Luna Dark only for v1 |
| DL-042 | AI memory | Dynamic: Presence Engine < 200 MB, LLM = total_ram/4 |
| DL-045 | OS name | Mahina (was LunaOS) |
| DL-046 | Default LLM | Qwen2.5 3B Q4_K_M via Ollama |
| DL-049 | luna-ai-d lang | Python v1 + asyncio |
| DL-050 | Reading font | Inter (SIL OFL) |
| DL-051 | Icon set | Phosphor Icons (MIT, 24px, Regular weight) |

---

## Repository Structure

```
mahina-os/
├── Makefile                    # Root build system (clang, static linking)
├── README.md                   # Project overview
├── ROADMAP.md                  # Phase overview (non-authoritative; see DCKL VII)
├── CHANGELOG.md                # Version history (v0.1, v0.2)
├── Mahina_DCKL_Complete.md     # Monolithic concat of all DCKL docs (AI context aid)
│
├── boot/
│   └── limine.conf             # Bootloader config (kernel + initramfs paths)
│
├── kernel/
│   ├── .config                 # Kernel config fragment for merge_config.sh
│   └── .config.notes           # Justification for every CONFIG_ option
│
├── etc/luna/
│   ├── hostname                # System hostname ("mahina")
│   ├── fstab.toml              # Filesystem mount table (TOML format)
│   ├── sysctl.toml             # Kernel tuning parameters
│   └── services/               # Service definitions (one .toml per service)
│       ├── dbus.toml           # D-Bus system message bus
│       ├── lgp-compositor.toml # LGP display compositor
│       ├── luna-ai-d.toml      # LUNA AI daemon
│       ├── networkmanager.toml
│       ├── ntpd.toml
│       ├── ollama.toml
│       ├── pipewire.toml
│       └── wireplumber.toml
│
├── src/
│   ├── luna-init/              # PID 1 init system (20 C files)
│   │   ├── main.c              # Entry point: 7-stage boot sequence
│   │   ├── toml.c/.h           # Minimal TOML parser (custom, fuzz-tested)
│   │   ├── service.c/.h        # Service file parser + state machine
│   │   ├── depgraph.c/.h       # Kahn's algorithm dependency graph
│   │   ├── supervisor.c/.h     # Service spawn, readiness poll, restart
│   │   ├── mount.c/.h          # fstab.toml mount manager
│   │   ├── log.c/.h            # Early logger (boot + runtime logs)
│   │   ├── signal.c/.h         # signalfd-based signal handler
│   │   ├── reaper.c/.h         # Zombie process reaper (waitpid -1 WNOHANG)
│   │   ├── shutdown.c/.h       # Orderly shutdown/reboot (stop → unmount → reboot(2))
│   │   ├── panic.c/.h          # Emergency shell fallback on PID 1 fatal error
│   │   ├── console.c/.h        # Welcome banner + interactive shell drop
│   │   ├── hostname.c/.h       # sethostname() from /etc/luna/hostname
│   │   ├── splash.c/.h         # luna-splash process launcher + IPC pipe
│   │   └── ctl.c/.h            # Unix socket control server (JSON protocol)
│   │
│   ├── luna-init-ctl/          # Control CLI (1 C file)
│   │   └── main.c              # Connects to /run/luna-init.sock, sends commands
│   │
│   └── luna-splash/            # Boot graphics engine (4 C files)
│       ├── main.c              # epoll event loop, signal handling
│       ├── render.c/.h         # /dev/fb0 framebuffer renderer
│       ├── ipc.c/.h            # Pipe-based IPC message reader
│       └── font8x16.h          # Embedded 8×16 bitmap font (256 glyphs)
│
├── tests/
│   ├── vendor/unity/           # Unity test framework (MIT)
│   ├── unit/luna-init/         # Active unit tests (4 files, 1 test each)
│   │   ├── log_test.c          # Tests log_init/write
│   │   ├── toml_test.c         # Tests basic TOML parsing
│   │   ├── depgraph_test.c     # Tests linear dependency resolution
│   │   └── service_test.c      # Tests service_find()
│   ├── unit/                   # STALE test files (not in TEST_SOURCES)
│   │   ├── test_toml.c         # Older version — superseded
│   │   └── test_depgraph.c     # Older version — superseded
│   └── fuzz/toml/
│       └── fuzz_toml.c         # AFL++ fuzzing harness for TOML parser
│
├── scripts/
│   ├── build-kernel.sh         # Kernel build wrapper
│   ├── build-initramfs.sh      # Initramfs creation (luna-init + busybox)
│   ├── build-image.sh          # Disk image creation (limine + initramfs)
│   └── run-qemu.sh             # QEMU launch helper
│
├── docs/
│   ├── DCKL/                   # Divine Collection of Knowledge about Luna (7 volumes)
│   │   ├── Volume I - Foundation/
│   │   ├── Volume II - Architecture/
│   │   ├── Volume III - Graphics & Presence/
│   │   ├── Volume IV - AI & Presence/
│   │   ├── Volume V - Userland & SDK/
│   │   ├── Volume VI - Development Bible/
│   │   └── Volume VII - Implementation Roadmap/
│   ├── Architecture Reviews/    # 6 AR sessions + 2 sprint reports
│   └── System Prompts/          # AI agent session templates
│
└── .github/workflows/
    ├── ci.yml                  # ACTIVE: 4-gate CI pipeline
    └── build.yml               # STALE: legacy workflow (should be deleted)
```

---

## Implemented Subsystems

### luna-init (COMPLETE)

The core of Stage 0. Every designed subsystem in Volume II/04 is implemented:

- **Logging:** Boot log (CLOCK_MONOTONIC_RAW timestamps) and runtime log (ISO 8601). Async-signal-safe path via `write(2)` only. `fsync()` on FATAL entries only.
- **Signal handling:** `signalfd` in epoll event loop. SIGCHLD→reap, SIGTERM→poweroff, SIGINT→reboot, SIGHUP→reload, SIGUSR1→dump state.
- **Zombie reaping:** `waitpid(-1, WNOHANG)` loop on every SIGCHLD.
- **Filesystem mounting:** Parses `fstab.toml`, mounts in order. `/proc`, `/sys`, `/dev` are critical (failure = panic). Others are warnings.
- **TOML parser:** Custom C17, no malloc in parser, 64KB document limit, bounded by `TOML_MAX_ENTRIES=128`. Supports strings, integers, booleans, string arrays, `[[array-of-tables]]`.
- **Service loading:** Reads `/etc/luna/services/*.toml`, parses all fields including dependencies, restart policy, readiness method, identity (user/group).
- **Dependency graph:** Kahn's algorithm. Detects cycles. Logs the precise set of services forming a cycle.
- **Service supervisor:** Topological start order. Binary-exists check before fork. `wait_for_ready()` via file/socket/HTTP(stubbed)/signal(stubbed) polling. Restart on failure.
- **Shutdown:** Reverse-order service stop → SIGTERM → timeout → SIGKILL. Unmount filesystems in reverse. `sync()`. `reboot(2)`.
- **Panic handler:** Drops to emergency shell on `/dev/tty1`. Tries `/bin/busybox`, `/bin/sh` etc. Spins if none found.
- **Control socket:** Unix domain socket at `/run/luna-init.sock`. Newline-delimited JSON. Commands: `list`, `status`, `start`, `stop`, `shutdown`, `reboot`.
- **luna-splash bridge:** Forks `/sbin/luna-splash --fd <N>` with a pipe. Sends `PERCENT|MSG\n` text updates via pipe write.

### luna-splash (COMPLETE)

Boot graphics engine. Runs as a child of luna-init.

- Opens `/dev/fb0`, mmap's the framebuffer.
- Renders 8×16 embedded bitmap font glyphs (256 ASCII glyphs in font8x16.h).
- Draws centered "MAHINA" logo, progress message, and filled progress bar.
- Epoll loop: reads pipe for `PERCENT|MSG\n` IPC, handles SIGTERM/SIGINT via signalfd.
- Clears screen before each update (brute-force pixel loop — functional but slow).
- No `malloc()` in the render path.

### luna-init-ctl (COMPLETE, basic)

CLI tool that connects to `/run/luna-init.sock` and sends JSON commands. Returns JSON responses. Used to inspect service states during development.

---

## Remaining Milestones

### Phase 1: Boot Aesthetics (PARTIALLY COMPLETE)
- ✅ luna-splash: framebuffer renderer, IPC, progress bar
- ⬜ Clean splash→compositor handoff without black screen flash (DL-043 documents the accepted 1-frame cut)
- ⬜ Block character / pixel art logo (current render falls back to text "MAHINA")

### Phase 2: The Graphics Layer (NOT STARTED)
All blocked on designing and implementing the LGP compositor. Decisions are complete (DL-025, DL-026, DL-029–034, DL-037–040). No code exists.

Required deliverables:
- `lgp-compositor` — KMS/DRM modesetting, LGP socket server, libinput, software renderer first
- `lgp-render` — Abstraction layer isolating GPU backend
- `LunaGUI` — Widget toolkit (flexbox layout, FreeType+HarfBuzz text, Phosphor Icons)
- `luna-island` — Ambient AI presence indicator
- `luna-shell`, `luna-bar` — Desktop shell
- `luna-lock` — Screen lock (DL-035)

### Phase 3: AI & Shell Integration (NOT STARTED)
- `luna-ai-d` — Python asyncio daemon, Ollama client, personality engine (DL-049)
- `lpkg` — Package manager with Ed25519 signing, Btrfs snapshot before updates (DL-048)

### Phase 4: Public Release v1.0 (NOT STARTED)
- Security hardening, AppArmor profiles, seccomp filters
- Full AT-SPI2 accessibility (DL-040)
- Documentation freeze
- First public ISO

---

## Release Roadmap

| Version | Codename | Focus |
|---|---|---|
| v0.1 (current) | Waxing | luna-init + luna-splash. Boots to shell. |
| v0.5 | TBD | LGP compositor + basic desktop |
| v0.9 | TBD | LUNA AI runtime integration |
| v1.0 | TBD | Public release |
| v2.0 | New Moon | musl migration, Rust rewrites, ARM64 |

---

## Known Blockers

1. **No LGP implementation.** Phase 2 cannot begin without designing and implementing the LGP compositor. All decisions are accepted. Implementation is the blocker.
2. **No lpkg implementation.** Package management is entirely undocumented in code.
3. **stale CI workflow.** `.github/workflows/build.yml` is a stale duplicate of the old pre-4-gate CI and should be deleted. The active pipeline is `ci.yml`.
4. **luna-splash fallback path bug.** `splash.c` line 51 tries `./build/luna-splash` (wrong), should be `./build/luna-splash/luna-splash` (actual Makefile output path). Low severity during development.
5. **`run_user`/`run_group` parsed but not enforced.** `service.c` parses `[service.identity]` user/group fields into `service_t`, but `supervisor.c` never calls `setuid()`/`setgid()`. All services run as root. Security gap for production.
6. **Minimal test coverage.** 4 tests total (1 per file), each testing a single happy path. No negative tests, no edge cases. Critical paths in supervisor, mount, shutdown, panic have zero test coverage.
7. **`luna_log_switch_to_runtime()` never called.** The runtime log is permanently unused. Log always writes to boot.log format.

---

## Current Status per Subsystem

| Subsystem | Status | Notes |
|---|---|---|
| kernel/.config | Complete | Fragment. Merge with `merge_config.sh`. |
| limine boot config | Complete | Trivial; references vmlinuz-mahina |
| luna-init | ✅ Complete | All Stage 0 subsystems implemented |
| luna-splash | ✅ Complete | Functional; text logo only (block chars not rendered) |
| luna-init-ctl | ✅ Complete | Basic JSON CLI |
| Unit tests | Minimal | 4 tests, 1 each. Insufficient for CI gate confidence. |
| Fuzz tests | Exists | AFL++ harness for TOML parser. Corpus directory present. |
| CI pipeline | Active | 4 gates: build → unit tests → clang-tidy → fuzz regression |
| LGP compositor | Not started | All decisions accepted (DL-025, 026, 031, 032, 033) |
| LunaGUI | Not started | Decisions accepted (DL-029, 030, 040) |
| luna-shell | Not started | Documented in Volume V |
| luna-island | Not started | Documented in Volume VII/04 |
| luna-ai-d | Not started | Python, decisions accepted DL-046, 047, 049 |
| lpkg | Not started | Python, decisions accepted DL-003, 017, 018, 048 |
| AppArmor profiles | Not started | Kernel support compiled in; no profiles committed |
| luna-lock | Not started | DL-035 accepted |
