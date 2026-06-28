# REPOSITORY_HEALTH_REPORT.md — Mahina OS
**Principal Engineer Release Audit**
**Generated:** 2026-06-28 | **Scope:** Full repository audit prior to Phase 2 implementation

---

## Executive Summary

Mahina OS is a well-conceived, architecture-first operating system project with an exceptional documentation foundation. The Stage 0 implementation (luna-init, luna-splash) is complete, correct, and consistent with the DCKL specification in all major respects. The Documentation-First methodology has produced a codebase that is uncommonly deliberate and principled for its early stage.

However, several issues must be addressed before Phase 2 begins. The most significant are: (1) a critical security gap where service identity (user/group) is parsed but never enforced, (2) dangerously insufficient test coverage, and (3) two stale files that create CI confusion. None of these block Phase 2 design, but all block a credible Phase 4 (production release).

**Repository Maturity Score: 6/10**
Strong for a Stage 0 OS project. Held back by test coverage, the security identity gap, and the absence of any Phase 2+ code to audit.

---

## 1. Repository Maturity

### Strengths

**Documentation quality is exceptional.** The DCKL (7 volumes, 51 Decision Log entries, 6 Architecture Review sessions) is more thorough than most production OS projects at v1.0. Every major architectural choice has written rationale and recorded consequences. This is rare and valuable.

**Code quality in luna-init is high.** The C code follows a consistent style, uses meaningful names, comments authority references per the DCKL system, and handles errors explicitly throughout. The TOML parser, dependency graph, and supervisor are all correct implementations of their specified behavior.

**Toolchain discipline is strong.** `-Wall -Wextra -Werror -Wpedantic` enforced. Static linking for PID 1. ASan+UBSan on all test builds. AFL++ fuzzer for the parser. clang-tidy as a CI gate. These are production-grade standards applied from day one.

**Signal handling is correct.** Using `signalfd` in an epoll event loop (not `signal()`/`sigaction()` handlers) is the correct approach for PID 1. Race conditions in signal delivery are eliminated by design.

**The TOML parser is the best component.** Custom, bounded, no malloc in the hot path, with an AFL++ fuzzing harness and fuzz corpus. This is production-ready code that has been thought through carefully.

**Architecture decisions are complete for Phase 2.** All critical Phase 2 decisions (LGP wire format, GPU backend, compositor readiness, input backend, clipboard, screen lock, font rendering, layout engine, icons) are accepted and documented. Phase 2 can begin immediately without architectural ambiguity.

### Weaknesses

**Test coverage is insufficient for a system that will be PID 1.** 4 unit tests total, 1 per file, all happy-path only. Zero negative tests. Zero tests for supervisor, mount, shutdown, or panic — the most critical subsystems. The fuzz harness is good but covers only the TOML parser. Before Phase 2, this must be addressed.

**Stale files create confusion.** Two files that should not exist:
- `.github/workflows/build.yml` — stale, pre-4-gate CI; runs on every push to main alongside `ci.yml`, causing duplicate CI runs; contains `echo "Tests placeholder"` which is clearly a development artifact
- `tests/unit/test_toml.c` and `tests/unit/test_depgraph.c` — duplicate test stubs superseded by `tests/unit/luna-init/` equivalents; not in `TEST_SOURCES` but present in the repository

**Service identity enforcement is missing.** Service files define `[service.identity]` with `user` and `group` fields. `service.c` correctly parses these into `service_t.run_user` / `service_t.run_group`. But `supervisor.c` never calls `setuid()`/`setgid()` before `execve()`. All supervised services run as root. In the current Stage 0 build (which doesn't start real system services), this is harmless. But for Stage 4 when D-Bus, NetworkManager, PipeWire, and Ollama are actually supervised, running all of them as root is a security regression from standard Linux practice.

**The runtime log is permanently unused.** `luna_log_init()` accepts a `runtime_log_path` parameter, but `luna_log_switch_to_runtime()` is never called from `main.c`. The comment in `log.c` says "(stored externally by caller if needed)" suggesting this was intentional for Stage 0, but it should be tracked as a known gap, not silently present.

**luna-splash has a fallback path bug.** In `splash.c` line 51, the development-mode fallback tries `./build/luna-splash` but the Makefile places the binary at `./build/luna-splash/luna-splash`. This means the fallback always fails silently in development without the system-level path. Low severity but incorrect.

---

## 2. Architecture Risks

### Risk: Blocking supervisor during service startup

**Severity: ✅ Resolved**
**Location:** `supervisor.c:supervisor_start_all()`, called synchronously from `main.c` Stage 4

**Assessment:** Resolved. The supervisor has been refactored to an asynchronous state machine (`supervisor_pump()`) driven by a timerfd in the epoll event loop. Services start asynchronously without blocking PID 1.

### Risk: No cgroup assignment in supervisor

**Severity: ✅ Resolved**
**Location:** `supervisor.c:spawn_service()`

**Assessment:** Resolved. `mount_early()` now mounts `/sys/fs/cgroup`. `spawn_service()` successfully creates `/sys/fs/cgroup/luna-system.slice/<service>.service` and assigns the child PID to `cgroup.procs` immediately after fork().

### Risk: Blocking readiness check during restart (reaper_on_exit)

**Severity: ✅ Resolved**
**Location:** `supervisor.c:reaper_on_exit()`

**Assessment:** Resolved. `reaper_on_exit()` no longer calls `sleep_ms()` or blocks. It sets `state = SERVICE_STATE_PENDING` and calculates a `scheduled_start_ms` timestamp, which the async `supervisor_pump()` uses to delay the restart.

### Risk: luna-splash process not waited after splash_stop()

**Severity: Low**
**Location:** `splash.c:splash_stop()`

`splash_stop()` sends SIGTERM to luna-splash and sets `splash_pid = -1`, then relies on the zombie reaper to clean up. The reaper will eventually process the zombie, but there's a window where luna-splash is dying but still holding the framebuffer mapping while the main process tries to move to the next stage. In practice this is fine since the framebuffer is shared-memory mapped, but it's worth documenting.

### Risk: DL-P04 (License decision) marked Pending in decision_log

**Severity: ✅ Resolved**
**Location:** `docs/DCKL/Volume I - Foundation/decision_log.md`

**Assessment:** Resolved. DL-P04 was successfully closed and superseded by DL-052 (MIT License).

---

## 3. Maintainability

**Score: 8/10**

The code structure is excellent. Each `.c` file has a clear, bounded responsibility. Headers expose minimal surfaces. The comment system citing DCKL authority (e.g., `/* Authority: Volume II / 04_init_system.md */`) is a standout practice that makes it trivially easy to find the specification for any piece of code. The consistent naming convention across all modules makes the codebase readable as a whole.

The main maintainability concerns are:
1. The global service table (`g_services`, `g_service_count`) is a shared global. As luna-init grows more complex, this will require more careful disciplined access. Currently fine.
2. `supervisor.c` handles restart logic inline in `reaper_on_exit()`. As restart policies become more complex (backoff, jitter, cgroup assignment), this function will need refactoring.

---

## 4. Documentation Quality

**Score: 9.5/10**

The DCKL is exceptionally well-maintained for a project at this stage. The Decision Log with 51 entries, full rationale, and consequences is better than most production projects. The Architecture Reviews are thorough. The Sprint reports provide continuity context.

Minor issues:
- `DL-P04` (License) is stale — should be closed
- `Volume II / 04_init_system.md` describes cgroup assignment that isn't implemented
- The `Mahina_DCKL_Complete.md` at root is a useful AI context tool but may drift out of sync with individual volume updates if not maintained automatically

---

## 5. Implementation Quality

**Score: 7/10**

The implemented code (luna-init, luna-splash) is correct and principled. The TOML parser is the highest-quality component — bounded, tested, fuzzed. The supervisor is architecturally sound for Stage 0 but will need significant rework for async operation before Stage 4.

The main implementation quality concerns:
- Identity enforcement gap (high severity for production)
- Blocking event loop during startup (medium severity)
- Stub readiness methods that silently succeed (READY_HTTP, READY_SIGNAL)
- render_clear() O(W×H) performance (trivial for a boot splash, would be unacceptable in any real-time context)

---

## 6. Consistency Score

**Documentation ↔ Implementation Consistency: 8/10**

All major architectural decisions are reflected in the code. The code consistently cites its DCKL authority. The main inconsistencies are:
- Identity enforcement: documented, not implemented
- Cgroup assignment: documented, not implemented
- inotify watch on services dir: mentioned in kernel config notes, not implemented (SIGHUP reload works without it)
- Runtime log switch: function exists, never called
- luna_log_switch_to_runtime() parameter discrepancy: the function signature takes no path argument, but the log.c comment says "runtime_log_path stored externally by caller" — the path passed to luna_log_init() is silently discarded

---

## 7. Technical Debt

**Current technical debt is low for Stage 0.** The stubs are intentional and documented. The gaps are administrative (identity enforcement) or minor (fallback path bug). There is no legacy code, no copy-paste, and no unexplained complexity.

**Projected technical debt for Stage 2+:**
The synchronous supervisor will need to become async before it can manage real services reliably. Deferring this to Stage 2 is acceptable, but it must be designed before lgp-compositor (a slow-starting process) is supervised. This is the largest upcoming technical debt item.

---

## 8. Security Observations

**Current security posture: Acceptable for Stage 0 development only.**

1. **All services run as root.** Not acceptable for production. Identity enforcement must be implemented before Stage 4.
2. **No AppArmor profiles committed.** Kernel support is compiled in. Profiles must be created before any network-facing service (networkmanager, ollama) is supervised.
3. **No seccomp filters.** DL-020 mandates seccomp for third-party application isolation. Required before lpkg ships.
4. **/run/luna-init.sock has mode 0600.** Correct — only root can send control commands.
5. **TOML parser is the most security-critical input-handling component (PID 1 parsing service files).** It has AFL++ fuzzing and bounded buffers. This is the right level of rigor.
6. **luna-ai-d will be Python.** Python processes running as system daemons have a larger attack surface than C processes. The planned AppArmor + cgroup isolation (DL-020, DL-042) is necessary.

---

## 9. Scalability Observations

The current architecture is designed for a single-machine consumer desktop. It is not designed for:
- Multi-user systems (one user; no user session management)
- Network services or server workloads
- ARM64 (DL-009: x86_64 only for v1; v2 adds ARM64)

These are all correct non-goals for v1.

Within its design scope, the architecture scales appropriately:
- Service table max `SERVICE_MAX_COUNT = 32` is more than sufficient for v1's service set (~10 services)
- TOML_MAX_ENTRIES = 128 is sufficient for current config files
- The LGP compositor design (TLV wire, software→Vulkan GPU stages) is designed to scale to high-resolution displays

---

## 10. Missing Tests (Priority Ordered)

1. **`supervisor.c` — spawn_service, wait_for_ready, restart logic.** Highest priority. This is the most complex and failure-prone component in Stage 0.
2. **`mount.c` — mount_fstab with valid/invalid/missing fstab.** Critical boot path, zero tests.
3. **`toml.c` — error cases.** The happy-path test passes. Need: overflow test (document > 64KB), cycle in array tables, malformed syntax, truncated strings.
4. **`depgraph.c` — cycle detection.** The current test only validates linear order. Cycle detection (the entire point of the algorithm) is untested.
5. **`shutdown.c` — orderly sequence.** Cannot be tested in isolation easily, but the logic should be verified.
6. **`service.c` — service_load_all with a real directory.** The find-by-name test bypasses the parser entirely.
7. **`signal.c` — signal action mapping.** Should verify SIGCHLD→REAP, SIGTERM→SHUTDOWN mappings.
8. **`panic.c` — visual-only, difficult to unit test.** At minimum, verify SHELL_CANDIDATES ordering.
9. **`luna-splash/ipc.c` — buffer boundary behavior.** The buffered line reader has subtle edge cases.

---

## 11. Build Reproducibility

**Score: 7/10**

The build is reproducible given the same host environment (Ubuntu 24.04 LTS, clang). The CI uses a pinned Ubuntu 24.04 runner.

Gaps:
- The kernel is not built in CI — only checked that the config fragment exists
- The disk image build uses host `busybox`, `mtools`, `xorriso` which may vary across systems
- No `Cargo.lock` or `requirements.txt` (not yet needed, but Python dependencies will need locking when luna-ai-d ships)
- The `kernel/` directory contains only a config fragment, not the kernel source — contributors must obtain Linux 6.6.x separately

---

## 12. Future Risks

1. **Phase 2 complexity.** lgp-compositor + LunaGUI is a large parallel effort. The risk of architectural drift between the compositor and the DCKL specification is high. Recommend frequent Architecture Reviews during Phase 2.

2. **Python in a system daemon (luna-ai-d).** Python startup time, memory footprint, and GIL behavior under load must be benchmarked against the targets in `Volume VI / 04_benchmarks.md` (≤ 2s to READY, ≤ 80 MB idle). The asyncio model helps but doesn't eliminate the GIL concern for CPU-bound tasks.

3. **Ollama dependency.** Ollama is a pre-compiled binary from an external project. It represents the largest trust boundary in the system. Its memory model, update cadence, and API stability are outside Mahina's control. Plan a minimum-version policy and monitor Ollama's API for breaking changes.

4. **glibc → musl migration.** DL-007 plans a musl migration in v2. Rebuilding all packages for musl is a significant effort. The lpkg ecosystem and any compiled packages will need musl-compatible builds. This is correctly deferred but should not be forgotten.

5. **Btrfs snapshot timing during shutdown.** DL-023 requires persistent LUNA memory to be summarized and encrypted at shutdown. DL-027 requires a Btrfs snapshot before every update. If the system shuts down uncleanly (power loss), neither of these will complete. The recovery path from a partially-written memory summary or a missing pre-update snapshot is not yet documented.

---

## 13. Recommendations (Prioritized)

### Immediate (before Phase 2 implementation begins)

1. **Delete `.github/workflows/build.yml`** — stale, causes duplicate CI runs, contains "Tests placeholder"
2. **Delete `tests/unit/test_toml.c` and `tests/unit/test_depgraph.c`** — superseded by luna-init/ versions, not in TEST_SOURCES, create confusion
3. **Fix luna-splash fallback path:** `./build/luna-splash` → `./build/luna-splash/luna-splash` in `splash.c` line 51
4. **Close DL-P04** — add DL-052 confirming MIT License as final decision (MIT is already used everywhere)
5. **Add explicit `CONFIG_CGROUP_V2=y`** to `kernel/.config` (or document in `.config.notes` why it's implicit)

### Before Phase 4 (production)

6. **Implement identity enforcement** — `setuid()`/`setgid()` in `supervisor.c` before `execve()`; requires getpwnam()/getgrnam() lookups
7. **Implement async supervisor startup** — redesign supervisor_start_all() to be epoll-driven rather than blocking
8. **Implement cgroup assignment** — create service cgroup slices and assign processes before exec
9. **Expand test coverage** — add supervisor, mount, depgraph cycle, TOML error cases, and shutdown tests
10. **Call luna_log_switch_to_runtime()** — from main.c at Stage 7 (or whenever the runtime log should begin)
11. **Commit AppArmor profiles** — at minimum for dbus, networkmanager, ollama, luna-ai-d
12. **Add QEMU boot integration test** — run `make run-qemu-headless`, verify serial output contains expected boot markers
