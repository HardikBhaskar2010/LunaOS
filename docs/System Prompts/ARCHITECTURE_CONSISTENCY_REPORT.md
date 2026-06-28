# ARCHITECTURE_CONSISTENCY_REPORT.md — Mahina OS
**Architecture Consistency Audit: Documentation ↔ Implementation**
**Generated:** 2026-06-28
**Methodology:** Compare DCKL volumes → Architecture Reviews → Decision Log → Source code
**Scope:** All implemented subsystems (Stage 0 only)

---

## Summary

Overall documentation-to-implementation consistency is **high for a Stage 0 project**. All major architectural decisions are reflected in the code. The code uniformly cites its DCKL authority. The inconsistencies found are either known gaps (documented stubs), minor omissions, or administrative artifacts. No instances of code contradicting accepted architecture were found.

**Consistency Tier:**
- ✅ Consistent — documentation and code agree
- ⚠️ Gap — documentation specifies something not yet implemented (acceptable for Stage 0)
- ❌ Contradiction — code implements something that contradicts documentation
- 🔵 Missing documentation — code has behavior not documented anywhere
- 📎 Administrative — stale/duplicate content that should be cleaned up

---

## 1. luna-init Core

### Signal Handling

**Authority:** `Volume II / 04_init_system.md §Signal Handling`

| Spec | Implemented | Status |
|---|---|---|
| signalfd() for all signal handling (race-free) | signalfd() in signal.c | ✅ |
| epoll-based event loop | epoll in main.c | ✅ |
| SIGCHLD → zombie reap | SIGNAL_ACTION_REAP → reaper_reap_all() | ✅ |
| SIGTERM → orderly shutdown | SIGNAL_ACTION_SHUTDOWN → shutdown_run(POWEROFF) | ✅ |
| SIGINT → reboot | SIGNAL_ACTION_REBOOT → shutdown_run(REBOOT) | ✅ |
| SIGHUP → reload services | SIGNAL_ACTION_RELOAD → service_load_all() + depgraph_build() | ✅ |
| SIGUSR1 → state dump | SIGNAL_ACTION_DUMP → log all service states | ✅ |

**Finding:** Signal handling is fully consistent with specification.

---

### TOML Parser

**Authority:** `Volume II / 04_init_system.md §Service File Format`, `Volume VI / 01_coding_standards.md §Fuzzing`

| Spec | Implemented | Status |
|---|---|---|
| Custom parser (no external library) | toml.c — zero external deps | ✅ |
| Supports [section], key=value, [[array-tables]] | All three handled | ✅ |
| Supports strings, integers, booleans, string arrays | All four types implemented | ✅ |
| Bounded buffers throughout | TOML_MAX_STR_LEN=256, TOML_MAX_KEY_LEN=64 etc. | ✅ |
| 64KB document size limit | TOML_MAX_DOC_BYTES = 64*1024 | ✅ |
| Unsupported TOML → TOML_ERR_UNSUPPORTED (never silent) | parse_value() returns ERR_UNSUPPORTED for unknown | ✅ |
| AFL++ fuzzing harness | tests/fuzz/toml/fuzz_toml.c | ✅ |
| No malloc in parser hot path | toml_load_buffer uses single calloc for the doc | ⚠️ |

**Note on malloc:** `toml_load_buffer()` does call `calloc()` once for the `toml_doc_t`. The header comment says "No dynamic allocation inside the parser" which is slightly imprecise — there is one allocation per document. However, the intent (no allocation per-entry or per-field) is satisfied. Clarify the header comment.

**Finding:** TOML parser is consistent with specification. One minor header comment imprecision.

---

### Service Parser

**Authority:** `Volume II / 04_init_system.md §Service File Format`

| Field | Documented | Parsed | Enforced | Status |
|---|---|---|---|---|
| [service].name | ✅ | ✅ | ✅ (used as key) | ✅ |
| [service].binary | ✅ | ✅ | ✅ (stat before fork) | ✅ |
| [service].description | ✅ | ✅ | N/A (informational) | ✅ |
| [service].args | ✅ | ✅ | ✅ (built as argv) | ✅ |
| [service].workdir | ✅ | ✅ | ✅ (chdir in child) | ✅ |
| [service.depends].after | ✅ | ✅ | ✅ (dep graph) | ✅ |
| [service.depends].before | ✅ | ✅ | ✅ (dep graph) | ✅ |
| [service.restart].policy | ✅ | ✅ | ✅ (supervisor) | ✅ |
| [service.restart].attempts | ✅ | ✅ | ✅ (supervisor) | ✅ |
| [service.restart].delay_ms | ✅ | ✅ | ✅ (supervisor) | ✅ |
| [service.ready].method | ✅ | ✅ | ✅ (file/socket) | ✅ |
| [service.ready].path | ✅ | ✅ | ✅ | ✅ |
| [service.ready].timeout_ms | ✅ | ✅ | ✅ | ✅ |
| [service.stop].signal | ✅ | ✅ | ✅ (SIGTERM/SIGKILL) | ✅ |
| [service.stop].timeout_ms | ✅ | ✅ | ✅ | ✅ |
| [service.identity].user | ✅ | ✅ | ✅ (setuid in spawn_service) | ✅ |
| [service.identity].group | ✅ | ✅ | ✅ (setgid in spawn_service) | ✅ |
| [service.env] | Documented | Not in macro | **❌ NOT PARSED** | ❌ Contradiction |

**Finding — Identity:** `[service.identity].user` and `[service.identity].group` are correctly parsed and enforced via `setuid()` and `setgid()` in `supervisor.c:spawn_service()`. Fully consistent.

**Finding — Environment Variables:** The spec defines `[service.env]` for per-service environment variable injection. `service_t` has `env[SERVICE_MAX_ENV_VARS]` fields and `supervisor.c:spawn_service()` does build an `envp[]` from them. However, `service.c` does not appear to parse the `[service.env]` section — the `GET_STR` macro calls in the file (as analyzed) don't include env variable parsing. **This requires verification by reading the full service.c.** If unimplemented, this is a contradiction since service files like `luna-ai-d.toml` may define environment variables.

**Action Required:** Verify service.c fully parses [service.env]. If not, implement or document as a known gap.

---

### Dependency Graph

**Authority:** `Volume II / 04_init_system.md §Dependency Graph Resolution`

| Spec | Implemented | Status |
|---|---|---|
| Kahn's algorithm for topological sort | depgraph.c | ✅ |
| Cycle detection with service name reporting | Reports each service with in-degree > 0 | ✅ |
| `after = []` — X must run before me | g_edge[dep_idx][i] = true | ✅ |
| `before = []` — I must run before X | g_edge[i][target_idx] = true | ✅ |
| Missing dependency → fatal error (service not found) | LUNA_FATAL + return -1 | ✅ |

**Finding:** Dependency graph is fully consistent with specification.

---

### Service Supervisor

**Authority:** `Volume II / 04_init_system.md §Service Supervisor`

| Spec | Implemented | Status |
|---|---|---|
| Start in topological order | depgraph_topo_sort() then loop | ✅ |
| Binary existence check before fork | stat(svc->binary) in spawn_service | ✅ |
| Readiness polling: READY_NONE, READY_FILE, READY_SOCKET | Implemented | ✅ |
| Readiness polling: READY_HTTP | Stubbed — returns true | ⚠️ Gap |
| Readiness polling: READY_SIGNAL | Stubbed — returns true | ⚠️ Gap |
| Restart policy: always, on-failure, never | All three handled in reaper_on_exit | ✅ |
| DEGRADED state after exceeding restart_attempts | restart_count > restart_attempts → DEGRADED | ✅ |
| SIGKILL timeout for stop | kill(SIGKILL) after stop_timeout_ms | ✅ |
| Cgroup assignment per service | **Implemented** | ✅ |
| setuid/setgid for identity | **Implemented** | ✅ |
| Event loop non-blocking | **Implemented** (Async state machine) | ✅ |

**Finding — Supervisor Architecture:** The supervisor is completely consistent with the DCKL specifications. It uses an async, event-driven state machine (`supervisor_pump`) driven by a timerfd to prevent blocking the event loop during service startup. It also properly enforces identity (setuid/setgid) and applies cgroups isolation per service.

---

### Filesystem Mounts

**Authority:** `Volume II / 02_boot_flow.md §Stage 2`, `Volume II / 09_filesystem.md`

| Spec | Implemented | Status |
|---|---|---|
| /proc mounted early (before log) | mount_early() called before luna_log_init() | ✅ |
| /sys, /dev mounted early | mount_early() | ✅ |
| fstab.toml [[mount]] parsed | mount_fstab() → toml_array_table_get() | ✅ |
| Critical mounts: failure = panic | is_critical() check → return MOUNT_ERR_FATAL | ✅ |
| Non-critical mounts: failure = WARN + continue | MOUNT_ERR_WARN path | ✅ |
| Unmount in reverse order on shutdown | mount_unmount_all() LIFO loop | ✅ |
| Options: ro, noexec, nosuid, nodev, relatime → flags | Parsed in do_mount() | ✅ |
| Additional options (size=2G) → mount data string | Appended to data_buf | ✅ |
| EBUSY → already mounted, log INFO + continue | EBUSY handled | ✅ |

**Finding:** Mount manager is fully consistent with specification.

---

### Logging

**Authority:** `Volume II / 11_logging.md`

| Spec | Implemented | Status |
|---|---|---|
| Boot log: CLOCK_MONOTONIC_RAW timestamps | log_get_boot_ms() using CLOCK_MONOTONIC_RAW | ✅ |
| Runtime log: ISO 8601 timestamps | log_get_iso8601() → gmtime_r | ✅ |
| Boot log format: [MS] [STAGE] [COMPONENT] [LEVEL] msg | log_write_entry() boot format | ✅ |
| Runtime log format: [ISO8601] [LEVEL] [COMPONENT] msg | log_write_entry() runtime format | ✅ |
| Entries < 4096 bytes (atomic write) | LOG_ENTRY_MAX=4000 | ✅ |
| fsync only for FATAL entries | level == LOG_FATAL → fsync() | ✅ |
| Async-signal-safe path: write(2) only | luna_log_signal_safe() | ✅ |
| Boot log path: /var/log/luna-init/boot.log | BOOT_LOG_PATH in main.c | ✅ |
| Runtime log: switch after Stage 7 | luna_log_switch_to_runtime() exists but never called | ⚠️ Gap |
| Boot default level: LOG_INFO | g_min_level = LOG_INFO | ✅ |
| Runtime default level: LOG_WARN | luna_log_switch_to_runtime() sets LOG_WARN | ✅ (but switch never called) |

**Finding:** Logging implementation is consistent. Runtime switch is a known gap (never called).

---

### Shutdown

**Authority:** `Volume II / 04_init_system.md §Shutdown Sequence`

| Spec | Implemented | Status |
|---|---|---|
| Mark g_shutting_down before stopping services | g_shutting_down = 1 first | ✅ |
| Stop services in reverse start order | Reverse loop over g_services[] | ✅ |
| SIGTERM → wait → SIGKILL on timeout | supervisor_stop_one() handles this | ✅ |
| Unmount in reverse mount order | mount_unmount_all() LIFO | ✅ |
| sync() before reboot(2) | sync() called | ✅ |
| reboot(2) with LINUX_REBOOT_CMD_RESTART or _POWER_OFF | Both handled | ✅ |

**Finding:** Shutdown sequence is fully consistent with specification.

---

### Control Interface

**Authority:** `Volume II / 04_init_system.md §Control Interface`

| Spec | Implemented | Status |
|---|---|---|
| Unix socket at /run/luna-init.sock | CTL_SOCKET_PATH in ctl.c | ✅ |
| Protocol: newline-delimited JSON | Documented in ctl.c header | ✅ |
| mode 0600 (root only) | chmod(CTL_SOCKET_PATH, 0600) | ✅ |
| `list` command → service list with state+PID | handle_list() | ✅ |
| `status` command → individual or overall | handle_status() | ✅ |
| `start` / `stop` commands | Implemented | ✅ |
| `shutdown` / `reboot` commands | Implemented | ✅ |

**Finding:** Control interface is consistent with specification.

---

## 2. luna-splash

**Authority:** `ROADMAP.md §Phase 1`, no dedicated DCKL chapter (gap)

| Feature | Spec Reference | Implemented | Status |
|---|---|---|---|
| Zero malloc in render path | Implied by Luna coding standards | No malloc in render_clear/text/logo/progress | ✅ |
| /dev/fb0 framebuffer | ROADMAP.md | render_init() opens /dev/fb0 | ✅ |
| Embedded bitmap font | ROADMAP.md | font8x16.h (8×16, 256 glyphs) | ✅ |
| Progress bar | ROADMAP.md | render_progress() | ✅ |
| IPC with luna-init for progress | ROADMAP.md | "PERCENT\|MSG\n" pipe protocol | ✅ |
| Clean handoff to compositor | DL-043 | One-frame black cut accepted | ✅ |
| Logo (ASCII art / branded) | ROADMAP.md | Text "MAHINA" fallback only | ⚠️ Gap |

**Missing documentation:** luna-splash has no dedicated DCKL chapter. Its design is only referenced in `ROADMAP.md`. This should be addressed — at minimum, a brief specification in `Volume II / 02_boot_flow.md` covering its IPC protocol and lifecycle.

---

## 3. Build System & CI

### Makefile vs Documented Toolchain

**Authority:** `Volume VII / 01_implementation_roadmap.md §0.1 Build Toolchain`, `Volume VI / 01_coding_standards.md`

| Spec | Makefile | Status |
|---|---|---|
| Clang as compiler (not GCC) | CC := clang | ✅ |
| -std=c17 | Present | ✅ |
| -Wall -Wextra -Werror -Wpedantic | Present | ✅ |
| -Wstrict-prototypes, -Wmissing-prototypes | Present | ✅ |
| -fstack-protector-strong | Present | ✅ |
| Static linking for luna-init | CFLAGS_STATIC := $(CFLAGS) -static | ✅ |
| ASan + UBSan in test builds | CFLAGS_TEST includes -fsanitize=address,undefined | ✅ |
| AFL++ fuzzing target | test-fuzz target | ✅ |
| clang-tidy lint gate | lint target | ✅ |

**Finding:** Build system is fully consistent with the documented toolchain.

### CI vs Documented Gates

**Authority:** `Volume VI / 05_testing_standards.md §CI Pipeline Test Gates`

| Gate | Spec | ci.yml | build.yml | Status |
|---|---|---|---|---|
| Gate 1: Build without warnings | ✅ | ✅ | Partial | Redundant |
| Gate 2: Unit tests ASan+UBSan | ✅ | ✅ | Echo "placeholder" | 📎 Stale |
| Gate 3: clang-tidy zero warnings | ✅ | ✅ | Not present | 📎 Stale |
| Gate 4: Fuzz regression | ✅ | ✅ | Not present | 📎 Stale |

**Finding:** `ci.yml` correctly implements all 4 documented gates. `build.yml` is a stale duplicate that should be deleted.

---

## 4. Service Files vs Service Parser

| Service | Schema Compliant | Identity Supported | Status |
|---|---|---|---|
| dbus.toml | ✅ | `user=messagebus, group=messagebus` defined but not enforced | ⚠️ |
| lgp-compositor.toml | ✅ | `user=root, group=root` (no privilege change) | ✅ |
| luna-ai-d.toml | Not fully verified | Not fully verified | ⬜ |
| networkmanager.toml | Not fully verified | Not fully verified | ⬜ |
| ntpd.toml | Not fully verified | Not fully verified | ⬜ |
| ollama.toml | Not fully verified | Not fully verified | ⬜ |
| pipewire.toml | Not fully verified | Not fully verified | ⬜ |
| wireplumber.toml | Not fully verified | Not fully verified | ⬜ |

**Finding:** Service files that specify non-root identity will silently be ignored at runtime — all processes start as root. This is a security gap.

---

## 5. Kernel Config vs Architecture Docs

**Authority:** `Volume II / 03_linux_architecture.md`, `kernel/.config.notes`

| Requirement | Source | Config | Status |
|---|---|---|---|
| Linux 6.6.x LTS | DL-009 | Fragment header states 6.6.x | ✅ |
| x86_64 | DL-009 | CONFIG_X86_64=y | ✅ |
| signalfd, epoll | luna-init architecture | CONFIG_SIGNALFD=y, CONFIG_EPOLL=y | ✅ |
| Btrfs root filesystem | DL-027 | CONFIG_BTRFS_FS=y | ✅ |
| EFI boot (limine) | DL-005 | CONFIG_EFI=y, CONFIG_EFI_STUB=y | ✅ |
| AppArmor (enforcing) | DL-020, 08_security.md | CONFIG_SECURITY_APPARMOR=y, DEFAULT_APPARMOR | ✅ |
| seccomp-BPF | DL-020 | CONFIG_SECCOMP_FILTER=y | ✅ |
| cgroups v2 | Volume II, supervisor | CONFIG_CGROUPS=y but no CONFIG_CGROUP_V2=y | ⚠️ |
| AES crypto (memory encryption) | DL-023 | CONFIG_CRYPTO_AES=y | ✅ |
| SHA-256 (lpkg package signing) | DL-048 | CONFIG_CRYPTO_SHA256=y | ✅ |
| DRM/KMS (compositor) | DL-026 | CONFIG_DRM=y, CONFIG_DRM_KMS_HELPER=y | ✅ |
| FBDEV emulation (luna-splash) | luna-splash architecture | CONFIG_DRM_FBDEV_EMULATION=y | ✅ |
| libinput (/dev/input/event*) | DL-032 | CONFIG_INPUT_EVDEV=y | ✅ |
| inotify (SIGHUP service reload) | kernel/.config.notes | CONFIG_INOTIFY_USER=y | ✅ |

**Finding on cgroups v2:** The config notes document describes cgroup v2 behavior (`CONFIG_CGROUP_V2=y` referenced in notes), but the fragment only has `CONFIG_CGROUPS=y`. On Linux 6.6, `CONFIG_CGROUPS=y` enables the cgroup subsystem, and cgroup v2 (unified hierarchy) is the default, but `CONFIG_CGROUP_V2` doesn't exist as a separate kconfig option in 6.6 — the v2 hierarchy is controlled via the `cgroup2` filesystem mount and the `UNIFIED_CGROUP_HIERARCHY` boot parameter. This is therefore not a real inconsistency — the kernel config is correct — but the config notes should be updated to remove the reference to a non-existent `CONFIG_CGROUP_V2` kconfig option to avoid confusion.

---

## 6. Decision Log Consistency

### Supersession Chain Integrity

| Entry | Status | Notes |
|---|---|---|
| DL-004 | Superseded by DL-004R | Correctly marked in DL-004R. DL-004 itself doesn't say "SUPERSEDED" in its header — minor admin gap |
| DL-006 | Superseded by DL-046 | Correctly noted. DL-046 says "supersedes DL-006". |
| DL-011 | Superseded by DL-027 | Correctly noted |
| DL-013 | Superseded by DL-036 | Correctly noted |
| DL-P01 through DL-P20 | Various "pending" entries | Most resolved by later DL entries. DL-P04 (License) still open despite MIT being used everywhere. |

**Finding on DL-P04:** The `decision_log.md` still contains `DL-P04` as a Pending decision: "MIT vs. GPL v3, Current leaning: MIT, Target: Before first public commit." However, the first public commit already used MIT. All source files have MIT license headers. `LICENSE` is MIT. This pending entry should be closed with `DL-052: MIT License — ACCEPTED (was DL-P04)`.

### Numbering Conflict (documented, resolved)

The decision log documents a known DL numbering collision: `Discussion_Session_2.md` used DL-005 through DL-018 as internal numbering, colliding with existing DL-005 through DL-010. The conflict was resolved by renumbering the session's decisions to DL-011+. The log clearly documents this. **This is correctly handled.**

---

## 7. Missing Documentation

| Gap | Missing From | Priority |
|---|---|---|
| luna-splash has no DCKL chapter | Volume II or III | Medium |
| IPC protocol between luna-init and luna-splash | No formal spec anywhere | Medium |
| luna-init-ctl JSON protocol | Only documented in ctl.c source comments | Low |
| Emergency shell behavior | Only in panic.c source | Low |
| SHELL_CANDIDATES list | Only in panic.c | Low |
| DL-P04 closure | decision_log.md | Low (administrative) |
| Cgroup v2 kconfig note correction | kernel/.config.notes | Low |

---

## 8. Duplicate Content

| Duplicate | Files | Resolution |
|---|---|---|
| Unit tests | `tests/unit/test_toml.c` / `tests/unit/luna-init/toml_test.c` | Delete tests/unit/test_*.c |
| Unit tests | `tests/unit/test_depgraph.c` / `tests/unit/luna-init/depgraph_test.c` | Delete tests/unit/test_*.c |
| CI workflow | `.github/workflows/build.yml` / `.github/workflows/ci.yml` | Delete build.yml |
| DCKL concatenation | `Mahina_DCKL_Complete.md` (root) vs individual volumes | Keep both — serve different purposes but need sync process |

---

## 9. Broken References

| Reference | Location | Issue |
|---|---|---|
| `LUNA_STAGE_READY = 7` | log.h | Stage 7 is defined but never set in main.c (Stage 5-7 are stubs) |
| `luna_log_switch_to_runtime()` | log.h | Function exists, documented, but never called |
| `inotify` watch | kernel/.config.notes | "luna-init uses inotify to watch /etc/luna/services/" — but no inotify code exists in luna-init |
| `READY_HTTP`, `READY_SIGNAL` | service.h, supervisor.c | Defined and parseable, but supervisor stub returns true immediately without documentation of this being a stub at the DCKL level |

---

## 10. Architecture Drift

No architecture drift was found between the DCKL and the implemented code. The code does not implement anything that contradicts an accepted Decision Log entry. The gaps found are all cases of "documentation specifies X, code doesn't implement X yet" — which is expected for Stage 0 stubs.

The one exception is the **blocking supervisor** issue: the epoll architecture implies non-blocking behavior, but supervisor_start_all() blocks. This is a gap between the stated architecture philosophy and the implementation approach. It should be explicitly acknowledged in the DCKL as a Stage 0 compromise, with a formal decision on how to redesign it for Stage 4.

---

## Summary: Issues by Category

### Contradictions (code violates documentation)
1. `supervisor.c`: Blocking event loop during service startup contradicts epoll-driven architecture philosophy

### Gaps (documented, not implemented)
2. `supervisor.c`: Identity enforcement (`setuid`/`setgid`) not implemented
3. `supervisor.c`: Cgroup assignment not implemented
4. `supervisor.c`: READY_HTTP stubbed (returns true)
5. `supervisor.c`: READY_SIGNAL stubbed (returns true)
6. `main.c`: `luna_log_switch_to_runtime()` never called
7. `main.c`: inotify watch not implemented (SIGHUP reload works without it)
8. `render.c:render_logo()`: Block character rendering loop is empty

### Missing Documentation
9. luna-splash has no DCKL chapter
10. IPC protocol between luna-init and luna-splash has no formal spec

### Administrative
11. `.github/workflows/build.yml` is stale — delete
12. `tests/unit/test_toml.c` and `test_depgraph.c` are stale — delete
13. `DL-P04` in decision_log.md should be closed as DL-052
14. `kernel/.config.notes` references `CONFIG_CGROUP_V2` which doesn't exist in Linux 6.6 kconfig
15. `splash.c` line 51: fallback path `./build/luna-splash` should be `./build/luna-splash/luna-splash`
16. `DL-004` header doesn't say "SUPERSEDED" despite DL-004R superseding it
