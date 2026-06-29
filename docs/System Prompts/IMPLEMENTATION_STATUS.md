# IMPLEMENTATION_STATUS.md — Mahina OS
**Living Engineering Dashboard. Update after every major milestone.**
**Generated:** 2026-06-28 | **Version:** 0.1.0 (Waxing)

---

## Summary Dashboard

| Phase | Status | Progress |
|---|---|---|
| Phase 0: Core Foundation | ✅ COMPLETE | ~95% |
| Phase 1: Boot Aesthetics | 🔵 IN PROGRESS | ~70% |
| Phase 2: Graphics Layer | 🔵 IN PROGRESS | ~20% |
| Phase 3: AI & Shell | ⬜ NOT STARTED | 0% |
| Phase 4: Public Release | ⬜ NOT STARTED | 0% |

---

## Phase 0: Core Foundation

### luna-init — PID 1 Init System

**Status:** ✅ COMPLETE
**Completion:** ~95%
**Current Milestone:** Phase 0 / Stage 4 boot
**Dependencies:** Linux kernel, glibc, signalfd, epoll, waitpid
**Blockers:** None
**Test Coverage:** 8 unit tests across log, toml, depgraph, and service_find. Supervisor/mount/shutdown have zero test coverage.
**Documentation Coverage:** Fully documented in `Volume II / 04_init_system.md`

| Subsystem | Status | Notes |
|---|---|---|
| Logging (log.c) | ✅ Complete | Boot log (monotonic) + runtime log (ISO 8601). fsync on FATAL only. |
| Signal handling (signal.c) | ✅ Complete | signalfd + epoll. SIGCHLD/TERM/INT/HUP/USR1 mapped. |
| Zombie reaper (reaper.c) | ✅ Complete | waitpid(-1, WNOHANG) on SIGCHLD. Calls supervisor restart callback. |
| TOML parser (toml.c) | ✅ Complete | Custom C17. 64KB limit. strings/ints/bools/arrays/array-tables. Fuzz-tested. |
| Service parser (service.c) | ✅ Complete | All TOML fields parsed incl. identity user/group (not enforced). |
| Dependency graph (depgraph.c) | ✅ Complete | Kahn's algorithm. Cycle detection with service name reporting. |
| Service supervisor (supervisor.c) | ✅ Complete | Async state machine, cgroup v2 assignment, restart policy. |
| Readiness: READY_FILE | ✅ Complete | stat() poll until file exists. |
| Readiness: READY_SOCKET | ✅ Complete | connect() to Unix socket. |
| Readiness: READY_HTTP | ⚠️ Stubbed | Returns true immediately. Planned v0.5. |
| Readiness: READY_SIGNAL | ⚠️ Stubbed | Returns true immediately. Planned v0.5. |
| Identity enforcement | ✅ Complete | run_user/run_group parsed, setuid/setgid called before execve(). |
| Mount manager (mount.c) | ✅ Complete | Parses fstab.toml [[mount]] entries. Mounts /sys/fs/cgroup. |
| Hostname (hostname.c) | ✅ Complete | Reads /etc/luna/hostname, calls sethostname(). Falls back to "mahina". |
| Panic handler (panic.c) | ✅ Complete | Emergency banner on /dev/tty1. Tries busybox/sh candidates. Spins if none. |
| Shutdown (shutdown.c) | ✅ Complete | Stop services → unmount filesystems → sync() → reboot(2). |
| Console (console.c) | ✅ Complete | Welcome banner (ANSI art) + shell exec. |
| Control socket (ctl.c) | ✅ Complete | JSON protocol: list, status, start, stop, shutdown, reboot. |
| Splash bridge (splash.c) | ✅ Complete | Fork luna-splash with pipe. |
| Main event loop (main.c) | ✅ Complete | epoll(signalfd, ctl_fd, inotify_fd). Stages 5-7 are intentional stubs. |
| Runtime log switch | ✅ Complete | luna_log_switch_to_runtime() called before entering Stage 0 interactive shell. |
| inotify reload | ✅ Complete | inotify watch on services dir triggers reload on any changes. |

**Next Task:** None. Core init architecture is feature-complete for Phase 0/1.
**Estimated Complexity:** N/A

---

### luna-splash — Boot Graphics Engine

**Status:** ✅ COMPLETE
**Completion:** 100%
**Current Milestone:** Phase 1 / Boot Aesthetics
**Dependencies:** /dev/fb0 framebuffer, Linux DRM_FBDEV_EMULATION kernel config
**Blockers:** None
**Test Coverage:** Zero unit tests.
**Documentation Coverage:** Partially (ROADMAP.md; no dedicated DCKL chapter)

| Subsystem | Status | Notes |
|---|---|---|
| Framebuffer init (render.c) | ✅ Complete | /dev/fb0 mmap. FBIOGET_VSCREENINFO/FSCREENINFO ioctls. |
| Bitmap font (font8x16.h) | ✅ Complete | 256-glyph 8×16 embedded font. Covers ASCII. |
| Text rendering | ✅ Complete | render_text(), render_text_centered(). |
| Progress bar | ✅ Complete | render_progress(msg, percent). Filled rectangle. |
| Screen clear | ✅ Complete | Fast memory filling via pointer arithmetic loop. |
| Block char logo | ✅ Complete | Custom UTF-8 parsing to render specific block characters dynamically. |
| IPC reader (ipc.c) | ✅ Complete | Pipe FD, "PERCENT\|MSG\n" protocol, buffered line reader. |
| Signal handling (main.c) | ✅ Complete | SIGTERM/SIGINT via signalfd in epoll loop. |
| luna-splash launch | ✅ Complete | Launched by splash_start() in luna-init. |
| Fallback path | ✅ Complete | Fixed to ./build/luna-splash/luna-splash. |
| compositor handoff | ⬜ Future | Documented as 1-frame black cut (DL-043). Requires compositor (Phase 2). |

**Next Task:** None. Core bringup complete. Wait for Phase 2 for compositor handoff.
**Estimated Complexity:** N/A
---

### luna-init-ctl — Control CLI

**Status:** ✅ COMPLETE (basic)
**Completion:** ~80%
**Current Milestone:** Phase 0 tooling
**Dependencies:** /run/luna-init.sock (luna-init must be running)
**Blockers:** None
**Test Coverage:** Zero tests.
**Documentation Coverage:** Referenced in `Volume II / 04_init_system.md`

| Feature | Status |
|---|---|
| Connect to Unix socket | ✅ |
| Send JSON command | ✅ |
| Print JSON response | ✅ |
| Help text / usage | Not verified (not fully analyzed) |
| Shell completion | ⬜ Future |

**Next Task:** Verify all documented CTL commands are implemented and tested.

---

### Build System

**Status:** ✅ COMPLETE
**Completion:** 100%
**Dependencies:** clang, lld, llvm-ar, make, busybox, mtools, xorriso, parted, QEMU

| Target | Status |
|---|---|
| `make all` (luna-init + luna-splash + ctl) | ✅ |
| `make luna-init` (static binary) | ✅ |
| `make luna-splash` (static binary) | ✅ |
| `make test-unit` (ASan + UBSan) | ✅ |
| `make test-fuzz` (AFL++ regression) | ✅ |
| `make lint` (clang-tidy) | ✅ |
| `make image` (disk image via scripts/) | ✅ (Linux/WSL2 only) |
| `make run-qemu` (QEMU x86_64 UEFI) | ✅ (Linux/WSL2 only) |

---

### Kernel Configuration

**Status:** ✅ COMPLETE (fragment)
**Completion:** 90%
**Notes:** This is a config FRAGMENT using merge_config.sh, not a full `.config`. Requires `make defconfig` first on the Linux 6.6.x source tree.

| Config Domain | Status | Notes |
|---|---|---|
| x86_64, EFI, SMP | ✅ | PREEMPT_VOLUNTARY, HZ_250, NO_HZ_IDLE |
| Btrfs, ext4, proc/sys/tmp | ✅ | All needed filesystems |
| VirtIO (QEMU block/net/input/gpu) | ✅ | Required for QEMU testing |
| DRM/KMS, FB emulation | ✅ | Required by luna-splash and future compositor |
| Input (evdev, HID, USB) | ✅ | Required by libinput (Stage 3+) |
| Networking (TCP/IP, nftables) | ✅ | NetworkManager, Ollama |
| cgroups v2 | ⚠️ Partial | CONFIG_CGROUPS set; CONFIG_CGROUP_V2 not explicit but implied on modern kernels |
| AppArmor | ✅ | Compiled in, enforcing mode default |
| seccomp-BPF | ✅ | Required for DL-020 sandbox |
| signalfd, epoll, timerfd | ✅ | Required by luna-init event loop |
| Audio (HDA, virtio-sound) | ✅ | PipeWire Stage 4+ |
| ACPI, CPU freq (schedutil) | ✅ | Power management |

---

### Documentation

**Status:** ✅ COMPLETE (comprehensive)
**Completion:** ~92%
**Notes:** All 51 Decision Log entries accepted. All DCKL volumes populated. A few minor gaps remain.

| Volume | Status | Gap |
|---|---|---|
| Volume I — Foundation | ✅ | DL-P04 (License) still listed as Pending despite MIT being used |
| Volume II — Architecture | ✅ | Minor: kernel config notes don't have explicit CGROUP_V2 entry |
| Volume III — Graphics & Presence | ✅ | Complete after AR-006 sprint 2 |
| Volume IV — AI & Presence | ✅ | Complete after AR-006 sprint 2 |
| Volume V — Userland & SDK | ✅ | Complete after AR-006 sprint 2 |
| Volume VI — Development Bible | ✅ | Python coding section added in sprint |
| Volume VII — Implementation Roadmap | ✅ | Stale "provisional" tags cleaned |

---

## Phase 2: Graphics Layer — IN PROGRESS

The early `lgp-compositor` path is implemented and building. DL-053 supersedes
DL-025 for the LGP wire format; the code uses the canonical 6-byte TLV header
(`uint16_t type` + `uint32_t length`).

### lgp-compositor

**Status:** 🔵 IN PROGRESS
**Completion:** ~25%
**Current Milestone:** Phase 2
**Dependencies (accepted):** Linux DRM/KMS, libinput (DL-032), D-Bus (DL-031), TLV wire format (DL-053)
**Blockers:** Stage 6 shell/LunaGUI clients are not implemented. D-Bus Ready is still a documented gap.
**Next Task:** Boot QEMU and run `/usr/bin/lgp-test-client` to verify a committed shared-memory surface, then begin the LunaGUI client library.
**Estimated Complexity:** Very High (3–6 months)

| Subsystem | Status | Authority |
|---|---|---|
| KMS/DRM modesetting | ✅ Basic | DL-026 |
| Software renderer | 🔵 Minimal | DL-026 §Stage 2 |
| Vulkan backend | ⬜ | DL-026 §Stage 3 (after software renderer) |
| LGP Unix socket server | ✅ Basic | DL-053 |
| TLV wire framing | ✅ Basic | DL-053 |
| LGP_HELLO handshake | ✅ Basic | DL-053 |
| Window management | ⬜ | Volume III/03_compositor.md |
| libinput backend | ⬜ | DL-032 |
| D-Bus Ready signal | ⬜ | DL-031 |
| Clipboard extension | ⬜ | DL-033 |
| luna-splash handoff | ✅ Basic | DL-043 (accept 1-frame cut) |

| Surface lifecycle | Basic | `LGP_CREATE_SURFACE`, `LGP_DESTROY_SURFACE`, `LGP_COMMIT_BUFFER` |
| Shared-memory buffer commit | Basic | `SCM_RIGHTS`, `XRGB8888` |

### LunaGUI

**Status:** ⬜ NOT STARTED
**Completion:** 0%
**Estimated Complexity:** Very High
**Next Task:** After lgp-compositor socket is working, implement LunaGUI client library.

| Subsystem | Status | Authority |
|---|---|---|
| LGP client library | ⬜ | Volume III/04_lunagui.md |
| FreeType + HarfBuzz text | ⬜ | DL-029 |
| Flexbox layout engine | ⬜ | DL-030 |
| Phosphor Icons SVG renderer | ⬜ | DL-051 |
| Bitcount + Inter font loading | ⬜ | DL-028, DL-050 |
| Widget set (panels, buttons, etc.) | ⬜ | Volume III/04_lunagui.md |
| Keyboard navigation (AT-SPI2 v1) | ⬜ | DL-040 |
| Theme engine | ⬜ | DL-039 (Luna Dark only) |
| LGP_THEME_CHANGED handling | ⬜ | DL-037 |

### luna-shell, luna-bar, luna-island

**Status:** ⬜ NOT STARTED
**Completion:** 0%
**Estimated Complexity:** High
**Notes:** luna-island owns the conversation panel (DL-044). Three states: AMBIENT, COMPACT_PANEL, FULL_CONVERSATION.

---

## Phase 3: AI & Shell Integration — NOT STARTED

### luna-ai-d

**Status:** ⬜ NOT STARTED
**Completion:** 0%
**Language:** Python + asyncio (DL-049)
**Default model:** Qwen2.5 3B Q4_K_M via Ollama (DL-046)
**Estimated Complexity:** High

| Subsystem | Status | Authority |
|---|---|---|
| Presence Engine (always on) | ⬜ | DL-021, Volume IV/01_presence_engine.md |
| LLM Inference Engine (lazy) | ⬜ | DL-021, AP-002 |
| Ollama client integration | ⬜ | DL-006/DL-046 |
| Context engine | ⬜ | DL-022, Volume IV/03_context_engine.md |
| Persistent memory (encrypted) | ⬜ | DL-023 |
| Offline/DEGRADED mode | ⬜ | DL-047 |
| Voice/TTS (optional) | ⬜ | DL-041 |
| D-Bus interface | ⬜ | Volume IV/00_luna_runtime.md |

### lpkg

**Status:** ⬜ NOT STARTED
**Completion:** 0%
**Language:** Python v1 (DL-003)
**Signing:** Ed25519 via libsodium (DL-048)
**Estimated Complexity:** Very High

| Subsystem | Status | Authority |
|---|---|---|
| Package manifest parsing | ⬜ | Volume V/03_package_manager.md |
| Ed25519 signature verification | ⬜ | DL-048 |
| Per-user install (~/.local/) | ⬜ | DL-017 |
| System-wide install (/usr/) | ⬜ | DL-017 |
| Atomic transaction + rollback | ⬜ | DL-018 |
| Btrfs snapshot before update | ⬜ | DL-027 + DL-018 |
| Repository trust levels | ⬜ | DL-019 |
| Third-party sandbox | ⬜ | DL-020 |

---

## Phase 4: Public Release — NOT STARTED

| Item | Status |
|---|---|
| AppArmor profiles for all daemons | ⬜ |
| seccomp filters for all daemons | ⬜ |
| AT-SPI2 bridge (luna-a11y) | ⬜ |
| Hardware compatibility pass | ⬜ |
| Security audit | ⬜ |
| Documentation freeze | ⬜ |
| ISO build pipeline | ⬜ |

---

## Test Coverage Summary

| Module | Unit Tests | Coverage Quality |
|---|---|---|
| log.c | 1 test (init + write) | Minimal — no error paths |
| toml.c | 1 test (basic valid doc) | Minimal — no error cases, no overflow |
| depgraph.c | 1 test (linear A→B) | Minimal — no cycle test, no before= test |
| service.c | 1 test (find by name) | Minimal — no parse test, no load_all test |
| supervisor.c | 0 tests | Not tested |
| mount.c | 0 tests | Not tested |
| shutdown.c | 0 tests | Not tested |
| panic.c | 0 tests | Not tested |
| signal.c | 0 tests | Not tested |
| hostname.c | 0 tests | Not tested |
| console.c | 0 tests | Not tested |
| ctl.c | 0 tests | Not tested |
| splash.c | 0 tests | Not tested |
| luna-splash/render.c | 0 tests | Not tested |
| luna-splash/ipc.c | 0 tests | Not tested |
| TOML fuzzer | libFuzzer target with AFL++ path | Good — covers parser paths under mutation |

**Overall unit test coverage:** Very Low. 8 tests covering 4 modules. 11 modules have zero test coverage.

---

## CI Gate Status

| Gate | Workflow | Status |
|---|---|---|
| Gate 1: Build (-Werror) | ci.yml | ✅ Active |
| Gate 2: Unit Tests (ASan+UBSan) | ci.yml | ✅ Active |
| Gate 3: clang-tidy (zero warnings) | ci.yml | ✅ Active |
| Gate 4: Fuzz regression | ci.yml | ✅ Active |
| Stale legacy CI | build.yml | ⚠️ Should be deleted |
| QEMU boot test | — | ⬜ Not implemented |
