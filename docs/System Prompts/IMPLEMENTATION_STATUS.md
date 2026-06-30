# IMPLEMENTATION_STATUS.md — Mahina OS
**Living Engineering Dashboard. Update after every major milestone.**
**Generated:** 2026-06-30 | **Version:** 0.2.0 (Waxing)

---

## Summary Dashboard

| Phase | Status | Progress |
|---|---|---|
| Phase 0: Core Foundation | ✅ COMPLETE | ~95% |
| Phase 1: Boot Aesthetics | ✅ COMPLETE | ~100% |
| Phase 2: Graphics Layer — LGP & Compositor | ✅ COMPLETE | ~100% |
| Phase 2: LunaGUI Toolkit | ✅ COMPLETE | ~100% |
| Phase 2: Desktop Shell (`luna-shell`) | ✅ COMPLETE | ~100% |
| Phase 2: Installer | ✅ COMPLETE (UI) | 100% UI / 0% backend |
| Phase 2: Terminal | ✅ COMPLETE | ~100% |
| Phase 2: Core Applications | ✅ COMPLETE (P7) | 6/6 apps implemented |
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
| compositor handoff | ✅ Complete | DL-043 (1-frame black cut) to LGP display server. |

**Next Task:** None. Core bringup complete.
**Estimated Complexity:** N/A
---

### luna-init-ctl — Control CLI

**Status:** ✅ COMPLETE (basic)
**Completion:** ~80%
**Current Milestone:** Phase 0 tooling
**Dependencies:** /run/luna-init.sock (luna-init must be running)
**Blockers:** None

---

## Phase 2: Graphics Layer & Window Management

The display compositor and Window Manager (Desktop Shell) are complete and fully building. The protocol uses the canonical 6-byte TLV header (`uint16_t type` + `uint32_t length`) with security extensions.

### lgp-compositor

**Status:** ✅ COMPLETE
**Completion:** 100%
**Current Milestone:** Phase 2
**Dependencies:** Linux DRM/KMS, evdev, D-Bus, TLV wire format (DL-053)

| Subsystem | Status | Authority |
|---|---|---|
| KMS/DRM modesetting | ✅ Complete | DL-026 |
| Software renderer | ✅ Complete | Z-order + ARGB8888 alpha blending |
| Vulkan backend | ⬜ Future | Post-Phase 3 optimization |
| LGP Unix socket server | ✅ Complete | DL-053 |
| TLV wire framing | ✅ Complete | DL-053 |
| LGP_HELLO handshake | ✅ Complete | DL-053 (with root-uid capability enforcement) |
| Window management | ✅ Complete | privileged WM message processing (`wm.c` / `wm_client`) |
| evdev input backend | ✅ Complete | keyboard.c (evdev parsing) + mouse.c (coordinates clamped to display) |
| Clipboard extension | ✅ Complete | Global clipboard buffer with `LGP_CAP_CLIPBOARD` gate |
| luna-splash handoff | ✅ Complete | DL-043 |
| Surface lifecycle | ✅ Complete | `LGP_CREATE_SURFACE`, `LGP_DESTROY_SURFACE`, `LGP_COMMIT_BUFFER` |
| Shared-memory buffer commit | ✅ Complete | `SCM_RIGHTS`, `XRGB8888` |

---

### LunaGUI

**Status:** ✅ COMPLETE
**Completion:** 100%

| Subsystem | Status | Notes |
|---|---|---|
| lgui_application_t (core) | ✅ Complete | LGP connect, HELLO handshake, poll() event loop |
| lgui_window_t (surface) | ✅ Complete | CREATE_SURFACE, memfd + SCM_RIGHTS, COMMIT_BUFFER |
| lgui_canvas_t (rendering) | ✅ Complete | fill_rect, draw_text (PSF font), draw_rect_outline |
| Widget: Label | ✅ Complete | Text display |
| Widget: Button | ✅ Complete | Click callback, on_click routing |
| Widget: VBox | ✅ Complete | Vertical stack with padding |
| Widget: HBox | ✅ Complete | Horizontal stack with flex distribution |
| lgui_widget_destroy() | ✅ Complete | Recursive tree free (prevents memory leaks on page nav) |
| lgui_widget_set_size() | ✅ Complete | Explicit dimension override |
| lgui_application_quit() | ✅ Complete | Signal event loop to stop |
| Input event routing | ✅ Complete | POINTER_MOTION + POINTER_BUTTON → hit-test → on_click |
| Keyboard event routing | ✅ Complete | `LGP_MSG_KEYBOARD_KEY` routed to `focused_widget->on_key` |
| Widget hit-testing | ✅ Complete | lgui_widget_hittest() recursive depth-first search |
| Render system | ✅ Complete | Mahina Design System palette, VBox/HBox layout engines |
| LGUI_LAYER_* constants | ✅ Complete | Matches lgp-compositor layer definitions |
| Scroll container | ✅ Complete | Clipping-enabled container for layout trees |
| Canvas widget | ✅ Complete | Custom callback-driven drawing widget |
| Theme engine | ⬜ Future | DL-039 (Luna Dark only); color palette defined in render.c |
| Animation engine | ⬜ Future | Deferred post-Phase 3 |
| Image loading | ⬜ Future | No image widget yet; requires PNG/SVG loader |

---

### luna-shell (`luna-shell` / `luna-bar` / `luna-island`)

**Status:** ✅ COMPLETE
**Completion:** 100%
**Notes:** Registers as a window manager, controls application layouts, handles global key bindings.

| Subsystem | Status | Notes |
|---|---|---|
| Wallpaper Surface | ✅ Complete | Renders custom gradient on LGUI_LAYER_WALLPAPER |
| Top Bar Surface | ✅ Complete | Displays branding and time on LGUI_LAYER_SHELL |
| Window Placement | ✅ Complete | Cascading floating layout |
| Hotkey: Super+T | ✅ Complete | Launches `luna-terminal` via fork/exec |
| Hotkey: Alt+Tab | ✅ Complete | Cycles focus among active applications |
| Focus Cycling | ✅ Complete | Sends `LGP_MSG_WM_SET_FOCUS` to compositor |

---

### luna-installer

**Status:** ✅ COMPLETE (UI — backend stubbed)
**Completion:** 100% UI / 0% disk write backend

---

### luna-terminal

**Status:** ✅ COMPLETE
**Completion:** 100%

| Feature | Status | Notes |
|---|---|---|
| PTY (forkpty) | ✅ Complete | pty.c spawns /bin/sh |
| ANSI escape parser | ✅ Complete | State machine in ansi.c |
| Cursor movement (A/B/C/D/H/f) | ✅ Complete | |
| Erase sequences (J/K 0/1/2) | ✅ Complete | |
| SGR colors (8+8 ANSI palette) | ✅ Complete | |
| Bold, cursor show/hide | ✅ Complete | |
| Scrollback buffer | ✅ Complete | 2000-line circular buffer |
| PTY resize (TIOCSWINSZ) | ✅ Complete | |
| Cell grid rendering | ✅ Complete | Pixel-level rendering cell-by-cell using Canvas widget |
| Copy/paste | ✅ Complete | Wire-level copy/paste using `Ctrl+Shift+V` |
| Keyboard input | ✅ Complete | Keyboard events translated and piped into PTY |

---

### Core Applications (Priority 7)

**Status:** ✅ COMPLETE (all 6 implemented)

| Application | Status | Notes |
|---|---|---|
| luna-settings | ✅ Complete | 6 pages: Display, Network, Audio, Users, About |
| luna-files | ✅ Complete | POSIX file browser, directory navigation |
| luna-calc | ✅ Complete | 4-function calculator with real expression evaluation |
| luna-text | ✅ Complete | File viewer, page-based scroll, up to 2000 lines |
| luna-about | ✅ Complete | Reads live /proc data (CPU, RAM, kernel, hostname) |
| luna-tasks | ✅ Complete | /proc task manager, SIGTERM kill, Refresh |

---

## Phase 3: AI & Shell Integration — NOT STARTED

### luna-ai-d

**Status:** ⬜ NOT STARTED
**Completion:** 0%
**Language:** Python + asyncio (DL-049)
**Default model:** Qwen2.5 3B Q4_K_M via Ollama (DL-046)

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
