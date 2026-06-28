# LGP Implementation Plan
**Mahina OS — Luna Graphics Protocol, Stage 1 Implementation**
**Prepared by:** Principal Graphics Systems Engineer review
**Scope:** First implementation of `lgp-compositor` — skeleton, DRM/KMS modesetting, LGP Unix socket, TLV transport
**Status:** Draft for engineering sign-off. Contains open questions that require a Decision Log entry before specific later milestones (clearly marked).

---

## 0. How to read this document

This plan is the product of a full read-through of the Mahina DCKL (all seven volumes), the Architecture Reviews, the Decision Log (DL-001–DL-052), the System Prompts status documents, and the current Stage 0 source (`src/luna-init/`, `src/luna-splash/`, `etc/luna/services/lgp-compositor.toml`). It does not redesign Mahina, does not introduce Wayland/wlroots/Hyprland, and does not invent architecture beyond what is explicitly required to make already-accepted decisions executable.

Companion documents (Phase 4/5/7/8 deliverables):
- `LGP_BRINGUP_SEQUENCE.md` — UEFI → desktop, with exact ownership-transfer points
- `LGP_DIRECTORY_STRUCTURE.md` — `src/lgp-compositor/` layout
- `LGP_PROTOCOL_REVIEW.md` — TLV protocol sufficiency review, including two protocol-level inconsistencies found in the DCKL
- `LGP_RISK_ASSESSMENT.md` — technical and process risk register

---

## 1. Phase 1 — What was read

Read in full: `PROJECT_STATE.md`, `AI_CONTEXT.md`, `IMPLEMENTATION_STATUS.md`, `ARCHITECTURE_CONSISTENCY_REPORT.md`, `REPOSITORY_HEALTH_REPORT.md` (docs/System Prompts/); `core_laws.md`, `non_negotiables.md`, `decision_log.md` (all 52 DL entries plus AP-001/AP-002), `glossary.md`, `living_interface_design.md` (Volume I); `02_boot_flow.md`, `07_ipc.md`, `12_kernel_user_boundary.md`, `13_component_ownership.md`, `04_init_system.md` (Volume II); `01_lgp.md`, `02_rendering_pipeline.md`, `03_compositor.md`, `04_lunagui.md`, `06_theme_engine.md`, `08_window_objects.md`, `09_visual_language.md` (Volume III, in full or targeted-grep depth); `01_implementation_roadmap.md` (Volume VII); `Architecture_Session_4.md` (AR-004 — the session that produced DL-025 through DL-044) in full. Read in full: `src/luna-init/main.c`, `src/luna-init/splash.c`, `etc/luna/services/lgp-compositor.toml`, `src/luna-init/service.h` (readiness enum).

**Where Mahina currently reaches the framebuffer:** `luna-splash` (a child process forked by `luna-init` in `splash.c`) opens `/dev/fb0` directly (legacy fbdev/`DRM_FBDEV_EMULATION` path), mmaps it, and renders an 8×16 bitmap font + progress bar with no `malloc()` in the render path. `luna-init` talks to it over a pipe with a `PERCENT|MSG\n` text protocol. This is the entire graphics stack that exists today — there is no DRM/KMS user, no compositor, and no LGP socket anywhere in the current codebase. `lgp-compositor` does not exist as a binary, a service that boots reliably, or a single line of C.

**Where LGP takes ownership:** at the boundary defined in `Volume II / 12_kernel_user_boundary.md` — Layer 3 (Graphics). `lgp-compositor` is the only process permitted to open `/dev/dri/card0` (DRM/KMS) and the only process permitted to open `/dev/input/event*` (via `libinput`, DL-032). Below it: the kernel DRM/KMS subsystem. Above it: the LGP Unix socket at `/run/lgp/compositor.sock`, consumed by LunaGUI and direct-LGP clients.

---

## 2. Phase 2 — Architecture Verification

**Verdict: the DCKL is sufficient to begin the four Stage 1 engineering tasks in Phase 6 (skeleton, DRM/KMS modesetting, socket lifecycle, TLV transport). It is *not* sufficient to implement the Color Resolver, the Z-order/layer constant table, or D-Bus-based compositor readiness without first resolving the contradictions below.** None of the open items below block Stage 1. All are listed because Core Law VI (Documentation Is Code) and `AI_CONTEXT.md` Rule 1 require that doc/code conflicts be flagged, not silently resolved.

### 2.1 Blocking for later milestones — require a Decision Log entry before implementation

**A. The Color Semantic Contract has at least three incompatible definitions in the DCKL.**

`core_laws.md` Law III states the Color Semantic Contract is **locked** ("Changes require full project review") and defines exactly five colors:

| Color | Hex | Role |
|---|---|---|
| LUNA Blue | `#00D4FF` | Primary |
| LUNA Purple | `#7B2FBE` | Secondary |
| LUNA Pink | `#FF2D78` | Alert |
| LUNA Green | `#00FF88` | Success |
| LUNA Amber | `#FFB800` | Caution |

`glossary.md` independently confirms this exact five-color set (Blue, Purple, Pink, Green, Amber).

`Volume III / 01_lgp.md` defines the `LGP_SET_SEMANTIC_COLOR` wire enum as a **different** five-code set — no Blue, no Purple, two new codes added:

```c
LUNA_GREEN = 0x01, LUNA_PINK = 0x02, LUNA_AMBER = 0x03, LUNA_WHITE = 0x04, LUNA_VOID = 0x05
```

`Volume III / 06_theme_engine.md` and `Volume IV / 01_presence_engine.md` build on this second (White/Void) set, not the `core_laws.md` set — but `06_theme_engine.md` and `09_visual_language.md` then disagree with **each other** on the hex values for White and Void (`#F0F0FF`/`#1A1A2E` vs. `#E8E8FF`/`#3A3A5C`), and `09_visual_language.md` reintroduces `LUNA_BLUE` at yet a third hex value (`#4A9EFF`, matched by `Volume V / 02_terminal.md`, vs. `core_laws.md`'s `#00D4FF`).

`Volume VII / 01_implementation_roadmap.md` §2.1 lists a **fourth** variant — six codes: `LUNA_BLUE, LUNA_GREEN, LUNA_AMBER, LUNA_PINK, LUNA_VOID, LUNA_RED` (`LUNA_RED` appears nowhere else in the DCKL).

**This was never reconciled in AR-004** — the session that produced DL-025 through DL-044 does not mention color codes at all. There is no DL entry that supersedes `core_laws.md`'s table or formally extends it.

*Impact:* none on Stage 1 (transport/skeleton). Direct blocker for: `LGP_SET_SEMANTIC_COLOR` payload encoding, the compositor's Color Resolver, and the Theme Engine.
*Recommendation:* a new Decision Log entry (proposed `DL-053`) that either (a) amends `core_laws.md` Law III to the five-or-six-code set Volume III actually uses (since Volume III is the more recent, more detailed, and more consistently-referenced source across four documents), or (b) restates Volume III's enum in terms of the locked Blue/Purple/Pink/Green/Amber set and drops White/Void/Red. Either is a one-paragraph fix; what cannot continue is implementing against an enum that has four different definitions on disk. **Flagging, not deciding** — this is a `core_laws.md` amendment per its own stated amendment process, not an implementation decision.

**B. The Z-order/layer numbering disagrees between the two documents that define it.**

`Volume III / 03_compositor.md` (the document `08_window_objects.md` itself cites as authoritative) defines:

| Layer | Value |
|---|---|
| `LAYER_WALLPAPER` | 0 |
| `LAYER_APPLICATION` | 100 |
| `LAYER_SHELL` | 200 |
| `LAYER_OVERLAY` | 300 |
| `LAYER_NOTIFICATION` | 400 |
| `LAYER_LUNA_ISLAND` | 500 |
| `LAYER_SYSTEM_MODAL` | 600 |
| `LAYER_CURSOR` | 700 |

`Volume III / 08_window_objects.md`'s per-surface-type tables instead assign: `WALLPAPER`=100 (not 0), `APPLICATION_WINDOW`=300 (not 100), a `TOP_LAYER` type at 400 that `03_compositor.md` does not name, and a `NOTIFICATION_TOAST` type at **500** — the same value `03_compositor.md` reserves exclusively for `LUNA_ISLAND` ("No application surface can appear above it"). The two documents' surface-type-to-layer maps cannot both be correct.

Separately, `08_window_objects.md` line 157 cites `DL-033` as authority for the `LGP_CAP_LAYER_SHELL` capability — `DL-033` is actually the Clipboard Architecture decision. This looks like a copy/paste citation error; `LGP_CAP_LAYER_SHELL` has no DL entry of its own today.

*Impact:* none on Stage 1. Direct blocker for: the compositor's Surface Manager Z-order array and the `LGP_SET_LAYER` validation logic.
*Recommendation:* a short Architecture Review note picking one canonical table (recommend keeping `03_compositor.md`'s, since it is the document that also defines the Focus Manager's layer-based authorization rule) and correcting `08_window_objects.md` to match, plus fixing the `DL-033` citation.

### 2.2 Gaps that touch Stage 1 integration surface — addressed by this plan, flagged for sign-off

**C. DL-031 (D-Bus compositor readiness) has no implementation path in the current luna-init, and the already-committed service file does not use it.**

DL-031 states the compositor "signals readiness by publishing a D-Bus signal: `org.mahina.compositor.Ready`. Stage 6 services wait on this signal before connecting to the compositor socket." But:
- `luna-init` is required to be statically linked with zero dynamic dependencies ("Things AI must NEVER change" #10 in `AI_CONTEXT.md`), and has no D-Bus client code today.
- `src/luna-init/service.h`'s `ready_method_t` enum supports exactly `READY_NONE`, `READY_FILE`, `READY_SOCKET` (implemented) and `READY_HTTP`, `READY_SIGNAL` (stubbed — return true immediately). There is no `READY_DBUS`. `Volume II / 04_init_system.md`'s service-file schema documentation likewise only documents `none`/`file`/`socket`/`http`.
- The **already-committed** `etc/luna/services/lgp-compositor.toml` uses `method = "socket"`, `path = "/run/lgp/compositor.sock"` — i.e., the repository has already silently chosen socket-presence over D-Bus as the de facto readiness signal, without a superseding DL entry.

*Resolution adopted by this plan (see §6.3, Task 3, and `LGP_BRINGUP_SEQUENCE.md`):* split DL-031's single mechanism into two, since it conflated two different consumers:
1. **luna-init's own readiness gate** uses `READY_SOCKET` against `/run/lgp/compositor.sock` (already implemented, already configured, requires no new code in `luna-init`).
2. **The compositor still emits `org.mahina.compositor.Ready` over D-Bus** for consumers that already have a D-Bus client and want a push notification instead of a poll (`luna-shell`, `luna-island`) — this is unaffected and remains exactly as DL-031 specifies for that audience.

This is the minimum-scope reconciliation: it keeps the static, dependency-free PID 1 guarantee, requires zero new luna-init code, and does not contradict any other accepted decision. **It should still be ratified as a DL amendment** (proposed `DL-054`, "Compositor readiness: socket for luna-init supervision, D-Bus broadcast for shell consumers") before Task 3 is merged, so the deviation from DL-031's literal text is recorded rather than silent.

**D. There is no Stage 5 hook in `luna-init`, and the splash→compositor ordering cannot be expressed by the existing dependency graph.**

`src/luna-init/main.c` has no code for boot Stages 5, 6, or 7 at all — the event loop goes directly from `supervisor_is_boot_complete()` (end of Stage 4) to `splash_stop()` and a shell fork. `Volume II / 02_boot_flow.md` and DL-043 require that `luna-splash` be SIGTERM'd and *fully exited* before the compositor opens `/dev/dri/card0` (this protects against the kernel's fbdev-emulation helper holding an implicit DRM master grab via `/dev/fb0`, which is released only when the fbdev character device is closed). `luna-splash` is not a TOML-managed service — it is a hardcoded `fork()`/`execve()` in `splash.c` — so this ordering constraint cannot be expressed via the `[service.depends]` `after =` mechanism the way `lgp-compositor.toml`'s `after = ["dbus"]` is.

*Resolution adopted by this plan:* a new, explicit Stage 5 block in `main.c`, inserted after the existing `splash_stop()` call (which already performs a synchronous `waitpid()` — confirmed by reading `splash.c`; `REPOSITORY_HEALTH_REPORT.md`'s claim that the splash PID is not waited is stale relative to the current code) and before the interactive-shell fork. See `LGP_BRINGUP_SEQUENCE.md` §3 for the exact sequencing. This is new code, not new architecture: it calls the same `supervisor_start_one()` primitive the supervisor already exposes, just for one specific service at one specific point in the boot timeline rather than as part of the generic Stage 4 batch start.

### 2.3 Non-blocking documentation staleness (corrected by reference, not requiring a DL entry)

| # | Document | Issue |
|---|---|---|
| 1 | `Volume II / 07_ipc.md` §6 | Still says "The LGP protocol specification has not been written... this is the highest-priority unresolved architectural decision." Volume III has since fully specified it (DL-025, DL-031, the socket path, the message catalogue). `AI_CONTEXT.md` tells agents to consult Volume II for IPC; this document should be updated to point to `01_lgp.md` rather than re-describing LGP as TODO. |
| 2 | `Volume II / 02_boot_flow.md` "Open Questions" #1 and "AI Context" bullet | Both still pose "should the splash be in-process or a separate process?" as unresolved, and the AI Context bullet asserts the in-process direction is current. The same document's own Stage 3 and Stage 5 body text, plus the shipped `splash.c`, already settled this as a separate forked process. The open question and the AI Context bullet are stale relative to the rest of the same file. |
| 3 | `Volume III / 01_lgp.md` "Current Decisions" table and "AI Context" section | The table marks the socket transport and surface-type list as "Provisional" and a TODO block asks whether to use a single socket vs. per-session sockets — but every other document that touches this (the service TOML, `03_compositor.md`, `Volume VII` roadmap, `kernel_user_boundary.md`) unanimously and consistently uses the single-socket model at `/run/lgp/compositor.sock` with no per-session variant referenced anywhere. The same document's "AI Context" section separately says "the wire format is TBD... do not implement until decided," contradicting its own Protocol Design section two pages earlier, which correctly cites DL-025 as accepted. |
| 4 | `Volume III / 01_lgp.md` "Current Decisions" table, GPU backend row | Says "GPU backend: Vulkan primary, OpenGL/EGL fallback | DL-026 | Accepted" without mentioning the Stage 2 (software renderer) / Stage 3 (Vulkan) staging that DL-026's actual text mandates and that `02_rendering_pipeline.md` correctly describes. Incomplete, not wrong. |
| 5 | `REPOSITORY_HEALTH_REPORT.md` §2 | Lists "luna-splash process not waited after splash_stop()" as a Low-severity open risk. The current `splash.c` calls `waitpid(splash_pid, NULL, 0)` synchronously inside `splash_stop()`. This appears to already be fixed; the report is stale on this specific point. |
| 6 | `main.c` Stage labeling | `boot_flow.md` documents the splash spawn as item 4 of "Stage 3 — Early Hooks." The shipped `main.c` calls `splash_start()` inside the code block labeled `STAGE 1`, before Stage 2's filesystem mounts. Cosmetic, but worth a one-line doc/code sync so the stage numbers in the log output and the stage numbers in the DCKL agree. |

None of items 1–6 require a decision; they require an edit. They are listed so the implementation work in this plan is not blamed later for "introducing" a discrepancy that already existed in the docs.

---

## 3. Phase 3 — Milestone Roadmap

Milestones are scoped to be independently completable and independently testable, per the engineering standard. M0–M3 are this plan's actual scope (Phase 6 below expands M1–M3 to full task design). M4+ are named so the roadmap is visible but are explicitly **out of scope for this plan** — they are not designed here and must not be started until M0–M3 are done and reviewed.

### M0 — `lgp-compositor` project skeleton
**Objective:** A buildable, lintable, testable, empty compositor binary that boots under `luna-init` and exits cleanly, with the project layout from `LGP_DIRECTORY_STRUCTURE.md` in place.
**Deliverables:** `src/lgp-compositor/` tree per the directory plan; `Makefile` integration (`make lgp-compositor` target, added to `make all`); structured logging wired to `/var/log/luna-init/runtime.log` per `03_compositor.md` §Compositor Log; signal handling (`signalfd` + `epoll`, matching the pattern already proven in `luna-init`); a `--version`/`--help` CLI surface; clang-tidy and `-Wall -Wextra -Werror -Wpedantic` compliance from the first commit (Volume VI / 01_coding_standards.md).
**Dependencies:** none beyond the existing toolchain.
**Complexity:** Low.
**Risks:** establishing bad patterns here (e.g., any `malloc()` in what will become the hot path) is expensive to unwind later — see `LGP_RISK_ASSESSMENT.md` R-9.
**Test strategy:** unit tests for argument parsing and log initialization; integration test: `luna-init` starts the binary, it logs a startup line, exits 0 on SIGTERM.
**Definition of Done:** `make all` builds `lgp-compositor`; `luna-init` (with `lgp-compositor.toml`'s `binary` path satisfied) starts it as a Stage 4-pattern service and observes clean SIGTERM shutdown; zero compiler/clang-tidy warnings.

### M1 — Raw DRM/KMS modesetting
**Objective:** The compositor takes exclusive ownership of the display, performs an atomic mode set, and presents a single solid-color frame (`LUNA Void`, pending §2.1-A resolution on the exact hex) via a dumb framebuffer — no GPU API calls (DL-026 Stage 2).
**Deliverables:** `drm/` and `kms/` modules (see directory plan) implementing device open, connector/CRTC enumeration, mode selection, dumb-buffer allocation, and a single page flip.
**Dependencies:** M0. Requires `lgp-compositor.toml`'s `[service.identity]` to keep `user=root` (DRM device node access) — already configured correctly in the committed service file.
**Complexity:** High (first real interaction with kernel DRM/KMS; correctness here gates everything downstream).
**Risks:** see `LGP_RISK_ASSESSMENT.md` R-1 (DRM ownership/master timing vs. `luna-splash`), R-3 (atomic modesetting fallback), R-2 (hotplug — explicitly deferred, see roadmap note below).
**Test strategy:** unit tests for connector/mode selection logic against mocked DRM ioctl results; integration test: boot in QEMU with `virtio-gpu`, observe a solid-color frame with no kernel fbcon bleed-through.
**Definition of Done:** QEMU display shows a stable, correctly-sized, correctly-colored frame with no tearing, no DRM ioctl errors in the log, and clean release of the DRM device on SIGTERM (verified by a second compositor instance being able to immediately re-acquire master).

### M2 — `/run/lgp/compositor.sock` lifecycle
**Objective:** The compositor opens, owns, and tears down the LGP Unix socket per the documented startup/shutdown sequence in `03_compositor.md`, independent of any message parsing.
**Deliverables:** `ipc/` socket-server module: `bind()`/`listen()` at `/run/lgp/compositor.sock`, `accept()` loop integrated into the M0 `epoll` loop, per-client connection state, clean unlink-and-recreate-on-restart behavior (required by the crash-recovery sequence in `03_compositor.md` §Crash Recovery step 3a).
**Dependencies:** M0. Logically independent of M1 (can be developed in parallel) but both must land before the readiness signal is meaningful end-to-end.
**Complexity:** Medium.
**Risks:** stale socket file from a previous crashed instance (`03_compositor.md` already specifies "clean up `/run/lgp/compositor.sock`" as restart step 3a — must be implemented as `unlink()`-before-`bind()` with `EADDRINUSE` handling, not assumed).
**Test strategy:** unit tests for socket setup/teardown and the stale-socket-cleanup path; integration test: a minimal test client connects, the compositor accepts and logs the session, disconnects cleanly; second integration test: kill `-9` the compositor mid-session, restart it, confirm the socket is recreated and a new client can connect within the DL-031-implied timeout window used by `lgp-compositor.toml` (`timeout_ms = 5000`).
**Definition of Done:** socket appears only after DRM/KMS is ready (ordering matters — see `LGP_BRINGUP_SEQUENCE.md`); `luna-init`'s `READY_SOCKET` poll against this path succeeds within the configured timeout under normal boot; survives the crash-restart cycle without manual cleanup.

### M3 — LGP TLV binary transport
**Objective:** A correct, fuzz-tested implementation of the DL-025 wire format (`uint8_t type`, `uint32_t length`, N-byte payload) plus the `LGP_HELLO`/`LGP_HELLO_REPLY` handshake — message *framing and handshake* only, no surface/buffer/rendering semantics yet.
**Deliverables:** `protocol/` module: TLV encode/decode with bounded buffers (matching the discipline already proven in `luna-init`'s `toml.c`), the `lgp_hello_t`/`lgp_hello_reply_t` structs exactly as specified in `01_lgp.md`, magic-number and version-compatibility-matrix validation, capability-flag intersection logic, and `SO_PEERCRED`-based policy hook (stubbed policy table acceptable at this milestone — full policy enforcement is a later milestone).
**Dependencies:** M2 (needs a connected socket to receive bytes on).
**Complexity:** Medium-High (correctness-critical, security-relevant: this is the parser that processes untrusted client input first).
**Risks:** see `LGP_RISK_ASSESSMENT.md` R-9 (buffer lifetime/parsing safety); this is the compositor's TOML-parser-equivalent in terms of "most security-critical input-handling component" and should receive the same AFL++ treatment `luna-init`'s TOML parser received.
**Test strategy:** AFL++ fuzz harness against the TLV decoder from day one (mirrors the existing `tests/fuzz/toml/` pattern); unit tests for every entry in the `01_lgp.md` version-compatibility matrix; integration test: a test client completes `LGP_HELLO` handshake and receives a correctly-populated `session_id`.
**Definition of Done:** fuzzer runs N hours with zero crashes/UBSan/ASan findings; full handshake compatibility matrix passes; a client sending a malformed/oversized TLV header is disconnected with `LGP_COMPOSITOR_ERROR` and the compositor itself does not crash or leak the connection's resources.

### M4+ (named for roadmap visibility only — not designed in this plan)
- M4: Surface Manager + `LGP_CREATE_SURFACE`/`LGP_DESTROY_SURFACE`/`LGP_COMMIT_BUFFER` (shm path only) — **blocked on §2.1-B** (layer numbering) before `LGP_SET_LAYER` can be validated correctly.
- M5: Frame callback model + damage tracking + software compositing pass.
- M6: Color Resolver + `LGP_SET_SEMANTIC_COLOR` enforcement — **blocked on §2.1-A** (Color Semantic Contract).
- M7: Animation engine + `LGP_SEND_MOTION` enforcement.
- M8: `libinput` integration + input routing (DL-032).
- M9: Crash recovery (`lgp_ext_recovery_v1`) end-to-end with a real Stage 6 client.
- M10: Stage 3 GPU backend (Vulkan primary / EGL fallback, DL-026) behind `lgp-render`.

---

## 4. Phase 4 — Bring-up sequence

See `LGP_BRINGUP_SEQUENCE.md` for the full UEFI-to-desktop sequence with exact ownership-transfer points. Summary of the four questions Phase 4 asks, answered here for convenience:

- **Framebuffer ownership transfer:** from `luna-splash` (fbdev, `/dev/fb0`) to `lgp-compositor` (DRM/KMS, `/dev/dri/card0`) at the moment `splash_stop()`'s `waitpid()` returns inside the new Stage 5 block in `main.c` — i.e., after the splash process has *fully exited*, not merely been signaled.
- **DRM master acquisition:** implicitly, on `open(O_RDWR)` of `/dev/dri/card0` as the first opener after `luna-splash`'s fbdev client has closed (M1).
- **KMS modesetting begins:** immediately after DRM master acquisition, inside M1's startup path, before the socket is created (M2 must come after M1 in the startup order, matching `03_compositor.md`'s documented sequence: DRM open → KMS configure → GPU backend init → framebuffer alloc → **then** socket creation).
- **Splash release:** `SIGTERM` sent synchronously by the new Stage 5 block, `waitpid()`-confirmed before M1 proceeds (already implemented correctly in `splash_stop()`; only the call site needs to move into a real Stage 5).
- **Compositor starts drawing:** after M1's first successful page flip — this is also the DL-043-accepted single black frame of "the brief cut."
- **Failure fallback:** per `02_boot_flow.md`'s Stage 5 row — "Fallback to framebuffer console" if KMS readiness isn't reached within the configured timeout. Concretely: `luna-init`'s `READY_SOCKET` poll already has a `timeout_ms` (5000, from the committed TOML); on timeout, the existing restart-policy machinery (`policy = "always"`, `attempts = 3`) already degrades the service per the existing `supervisor.c` state machine — no new failure-handling code is required for M0–M3, since `luna-init`'s supervisor was already built to handle exactly this shape of failure for any service. Only the Stage 5 main.c block needs to skip the shell-fork delay and proceed to drop to console (per `02_boot_flow.md`'s documented behavior) if the compositor enters `DEGRADED` rather than `RUNNING`.

---

## 5. Phase 5 — Directory structure

See `LGP_DIRECTORY_STRUCTURE.md`.

---

## 6. Phase 6 — Engineering design for the first four tasks

### 6.1 Task 1 — `lgp-compositor` project skeleton

**Required source files** (see `LGP_DIRECTORY_STRUCTURE.md` for full layout): `src/lgp-compositor/main.c`, `logging/log.c/.h` (thin wrapper reusing the `luna-init` log line format for `runtime.log` consistency), `util/signal.c/.h` (same `signalfd`-in-`epoll` pattern as `luna-init`), `config/args.c/.h` (CLI flags).

**Public interfaces:** none yet beyond the process boundary (argv, exit code, signals).

**Internal interfaces:** `int lgp_log_init(const char *runtime_log_path)`; `int lgp_signal_init(void)` returning a `signalfd`; `lgp_signal_action_t lgp_signal_read(int fd)`.

**Data structures:** a single top-level `lgp_compositor_state_t` struct (the eventual home for the epoll fd, DRM fd, socket fd, and surface list added in later milestones) — created here mostly empty, but its existence from M0 onward avoids the global-mutable-state pattern `REPOSITORY_HEALTH_REPORT.md` flagged as a maintainability watch-item in `luna-init`'s `g_services` table. Prefer passing `lgp_compositor_state_t *` explicitly through the call chain rather than file-scope globals.

**Thread model:** single thread at this milestone. The two-thread model (main + render) specified in `03_compositor.md` is introduced at M5 (render thread has nothing to do until there's a scene to render); building it now would be premature complexity.

**Failure handling:** any unrecoverable startup failure logs `FATAL` and exits non-zero — `luna-init`'s existing supervisor restart policy (already configured: `policy="always"`, `attempts=3`, `delay_ms=1000`) handles the retry/degrade logic; the compositor itself does not need its own restart loop.

**Logging strategy:** `INFO` for start/stop, `DEBUG` for signal received, matching the event/level table already defined in `03_compositor.md` §Compositor Log (this table is reused verbatim — do not invent new log levels).

**Unit tests:** CLI argument parsing; log file creation in a temp directory.
**Integration tests:** start under `luna-init` test harness (or directly via `luna-init-ctl start lgp-compositor`), confirm clean SIGTERM exit and a `runtime.log` entry.

### 6.2 Task 2 — Raw DRM/KMS modesetting

**Required source files:** `src/lgp-compositor/drm/device.c/.h` (open/close, capability query), `drm/connector.c/.h` (enumeration, mode selection), `kms/crtc.c/.h` (CRTC/connector association, atomic commit), `kms/dumb_buffer.c/.h` (dumb buffer alloc/map/free).

**Public interfaces:** `int drm_device_open(const char *path)`; `int drm_device_select_mode(drm_device_t *dev, drm_mode_t *out)`; `dumb_buffer_t *kms_dumb_buffer_create(drm_device_t *dev, uint32_t w, uint32_t h, uint32_t fourcc)`; `int kms_page_flip(drm_device_t *dev, dumb_buffer_t *fb)`.

**Internal interfaces:** connector/CRTC matching algorithm (mirrors the snippet already sketched in `02_rendering_pipeline.md` §DRM device discovery — `/dev/dri/card0` first, `/dev/dri/card1` fallback, exactly as documented); mode-selection heuristic (preferred mode flag from `drmModeGetConnector`, falling back to highest-refresh available mode — this specific tie-break rule is **not** specified anywhere in the DCKL and is this plan's own pragmatic default; flag for confirmation in code review, not a DL-blocking gap since it has no behavioral consequence beyond "which mode is picked when several are equally valid").

**Data structures:** `drm_device_t` (fd, resources, capabilities), `drm_connector_t`, `dumb_buffer_t` (handle, fb_id, stride, size, mapped pointer).

**Thread model:** main thread only (M0's single-thread model holds through M1 — the render thread is not needed until there is per-frame work to overlap with message processing).

**Failure handling:** `/dev/dri/card0` open failure → try `card1` → if both fail, log `FATAL` and exit (the supervisor degrade path takes over, matching the Stage 5 fallback in `02_boot_flow.md`). Atomic commit failure → log `ERROR` with the kernel error code, retry once with the legacy (non-atomic) `drmModeSetCrtc` path as a documented fallback (see `LGP_RISK_ASSESSMENT.md` R-3) — if that also fails, treat as a startup failure, not a runtime crash.

**Logging strategy:** `INFO` for "Compositor started, display configured" (already in the `03_compositor.md` log table) including the selected mode (resolution/refresh); `FATAL` for "DRM device lost" (already in the same table).

**Unit tests:** mode-selection logic against synthetic `drmModeConnector` fixtures (requires an injectable DRM interface — i.e., the public functions above must take their DRM ioctl calls through a thin function-pointer table or `libdrm` directly with a mockable wrapper, not inline `ioctl()` calls, specifically so this is testable without real hardware/KVM).
**Integration tests:** boot in QEMU with `-vga virtio` (matches the kernel config's `VirtIO GPU` support already present in `kernel/.config`); confirm a solid-color frame; confirm clean device release lets a second invocation re-acquire master.

### 6.3 Task 3 — Open and manage `/run/lgp/compositor.sock`

**Required source files:** `src/lgp-compositor/ipc/socket_server.c/.h` (bind/listen/accept), `ipc/client.c/.h` (per-connection state).

**Public interfaces:** `int lgp_socket_server_init(const char *path)`; `int lgp_socket_server_accept(int listen_fd)`; `void lgp_client_close(lgp_client_t *client)`.

**Internal interfaces:** stale-socket cleanup (`unlink()` before `bind()`, handling the case where the path exists but no process is listening — exactly the `03_compositor.md` crash-recovery step 3a, "Clean up `/run/lgp/compositor.sock`"); per-client structure registered in the M0 `epoll` set.

**Data structures:** `lgp_client_t { int fd; uint32_t session_id; struct sockaddr_un peer; pid_t peer_pid; uid_t peer_uid; }` — the `peer_pid`/`peer_uid` fields are populated via `SO_PEERCRED` at `accept()` time, required later (M3+) for the capability-flag policy check `01_lgp.md` specifies but needed structurally from the first connection onward.

**Thread model:** main thread; `accept()` is non-blocking and integrated into the existing `epoll` loop exactly as `luna-init`'s `ctl_server_accept()` pattern already demonstrates (the new code is structurally a sibling of that existing, working pattern, not a new design).

**Failure handling:** `bind()` failing with `EADDRINUSE` after the stale-socket cleanup attempt → log `FATAL` (something else genuinely holds the socket — do not silently `unlink()` and retry indefinitely, since that could race a legitimately-running second instance during a restart window). `accept()` failure on a single connection → log `WARN`, continue serving other clients (one bad client must never take down the compositor — this is the same principle `03_compositor.md` states for protocol violations: disconnect the offending client, not the server).

**Logging strategy:** `INFO` for "Compositor started, display configured" extended to also confirm socket creation (matches `03_compositor.md`'s step 7→8 ordering: socket created, *then* readiness signaled); `DEBUG` for connect/disconnect with session ID (both already in the `03_compositor.md` log table).

**Unit tests:** stale-socket cleanup logic; `SO_PEERCRED` extraction against a local test socket pair.
**Integration tests:** the M2 client-connect/disconnect test described above; the crash-restart-recreates-socket test described above; a concurrent-second-instance test confirming the second instance fails to bind while the first is alive (protects against two compositors fighting over DRM master, which would otherwise be a silent and very confusing failure mode).

### 6.4 Task 4 — LGP TLV binary transport (DL-025)

**Required source files:** `src/lgp-compositor/protocol/tlv.c/.h` (generic encode/decode), `protocol/hello.c/.h` (handshake-specific), `protocol/caps.c/.h` (capability flag table + negotiation).

**Public interfaces:** `int lgp_tlv_decode_header(const uint8_t *buf, size_t len, lgp_tlv_header_t *out)`; `ssize_t lgp_tlv_encode(uint8_t type, const void *payload, uint32_t payload_len, uint8_t *out_buf, size_t out_cap)`; `int lgp_hello_handle(lgp_client_t *client, const lgp_hello_t *hello, lgp_hello_reply_t *reply)`.

**Internal interfaces:** the version-compatibility matrix from `01_lgp.md` (`major == compositor_major` always accepted; `client_major == compositor_major - 1` accepted only if a back-compat shim exists — none does yet at v1.0, so this reduces to "exact major match required" for the current implementation, with the matrix's other rows wired up as the comments documenting *future* behavior, per the document's own versioning policy of append-only growth); capability-flag intersection (`negotiated = client_flags & compositor_supported_flags`); a policy table (initially a static allow-list keyed by binary path / `SO_PEERCRED` UID, sufficient for `LGP_CAP_LAYER_SHELL`/`LGP_CAP_LUNA_ISLAND` gating at this milestone — full AppArmor-profile-driven policy is a later milestone, not blocking here).

**Data structures:** `lgp_tlv_header_t { uint8_t type; uint32_t length; }`; `lgp_hello_t`/`lgp_hello_reply_t` exactly as specified in `01_lgp.md` (byte-for-byte, since the ABI stability policy in the same document makes these structures append-only from the moment they ship).

**Thread model:** main thread; parsing happens synchronously inside the `epoll` read callback for a client fd, with strict bounds checking before any length-prefixed read (mirrors the bounded-buffer discipline of `luna-init`'s TOML parser, which is explicitly called out in the DCKL as the reference example of this discipline).

**Failure handling:** any TLV header with `length` exceeding a hard ceiling (must be defined — `01_lgp.md` does not specify a maximum message size; recommend a conservative `LGP_MAX_MESSAGE_BYTES` constant, e.g. matching the largest payload any v1 message type requires plus headroom, defined in `protocol/tlv.h` and treated as the kind of bounded-constant decision `Volume VI` coding standards already expect — not a DL-blocking gap, since "reject anything absurd" is correct regardless of the exact number chosen) is rejected with `LGP_COMPOSITOR_ERROR` and the connection is closed; a client that fails the `LGP_HELLO` magic-number check or sends any message before `LGP_HELLO` is disconnected immediately, no error message required (it isn't speaking the protocol at all).

**Logging strategy:** `WARN` for any protocol violation resulting in disconnect (matches `03_compositor.md`'s "Motion Vocabulary violation (client disconnected) | WARN" pattern — protocol-framing violations get the same treatment).

**Unit tests:** every row of the version-compatibility matrix; capability-flag intersection truth table; TLV round-trip encode/decode for boundary lengths (0, 1, `LGP_MAX_MESSAGE_BYTES`, `LGP_MAX_MESSAGE_BYTES + 1`).
**Integration tests:** the AFL++ fuzz harness (see M3 above) as a CI gate, mirroring `tests/fuzz/toml/`; a real test client completing the full handshake against a running compositor instance in QEMU.

---

## 7. What this plan deliberately does not do

- It does not implement `LGP_SET_SEMANTIC_COLOR`, the Color Resolver, or any Theme Engine code — blocked on §2.1-A.
- It does not implement `LGP_SET_LAYER` validation or the Z-order array — blocked on §2.1-B.
- It does not implement `libinput` integration, the animation engine, GPU backends (Vulkan/EGL), DMA-BUF import, multi-monitor, or the crash-recovery extension end-to-end — all explicitly named as M4+ and out of scope here.
- It does not add any dependency not already accepted in the Decision Log (`libdrm` and `libinput` are the only new first-party dependencies introduced across M0–M3, and only `libdrm`-equivalent DRM/KMS ioctl usage is needed through M1–M3; `libinput` itself is not linked until M8).
