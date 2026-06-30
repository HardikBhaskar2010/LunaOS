# MAHINA_ENGINEERING_SNAPSHOT.md

**Repository:** `HardikBhaskar2010/MahinaOS`
**Snapshot derived from commit:** `591e280f57283966b6cf912a5a4df4a24078b519` ("Implement LGP surface commits", 2026-06-30 00:51:57 +0530)
**Total commits at time of analysis:** 35
**Method:** Full clone, line-by-line reading of every source file, build script, service definition, CI workflow, and DCKL document, cross-checked against `git log` history. Where the repository's own status documents disagree with the code, this snapshot trusts the code and says so explicitly.
**Role:** Principal Systems Engineer / Project Historian pass — this document does not propose architecture changes, does not introduce Wayland/wlroots/Hyprland, and does not invent unimplemented features.

---

## 0. Read This First — Documentation vs. Implementation

Mahina is developed "documentation-first," but as of this snapshot the project's own canonical status files have drifted from the code in specific, checkable ways. This isn't a new finding — the repository already contains two excellent self-audits (`docs/CODE_AUDIT_REPORT.md` and `docs/ARCHITECTURE_COMPLIANCE_REPORT.md`, both dated 2026-06-29) that caught most of this and triggered a large fix commit (`496451a`). This snapshot independently re-verified every claim against the current `HEAD` and is the only one of the status documents that reflects the **post-591e280** state.

| Document | Last updated | Currently accurate? |
|---|---|---|
| `docs/System Prompts/IMPLEMENTATION_STATUS.md` | commit `591e280` (HEAD) | **Yes** — the one living document that's actually current |
| `docs/System Prompts/AI_CONTEXT.md` | commit `591e280` (HEAD) | **Yes** — current |
| `docs/CODE_AUDIT_REPORT.md` / `ARCHITECTURE_COMPLIANCE_REPORT.md` | commit `7d02b43` (2026-06-29) | Mostly historical now — most findings were fixed in `496451a`; a few remain open (see §5) |
| `docs/System Prompts/PROJECT_STATE.md` | commit `34333af` (2026-06-28) | **Stale.** Predates 13 commits of work, including the *entire* `lgp-compositor` subsystem. Still says "LGP compositor: NOT STARTED / zero code" and lists five blockers that are already fixed in code. |
| `docs/System Prompts/ARCHITECTURE_CONSISTENCY_REPORT.md`, `REPOSITORY_HEALTH_REPORT.md` | predate `lgp-compositor` entirely | **Stale**, and per the compliance report, internally self-contradictory even at the time they were written |
| `ROADMAP.md` (root) | static | Marks "Phase 0: Core Foundation (Current)" — implementation is actually in **Phase 2 (Graphics Layer)** |

**Net effect:** if you only read `PROJECT_STATE.md` you would believe Mahina has no graphics code at all. It has a real, building, ~25%-complete DRM/KMS compositor with a working (if early) surface/window model. Treat `IMPLEMENTATION_STATUS.md` + this document as ground truth going forward; the other status docs need regeneration.

---

## 1. Current Project State

### 1.1 What Mahina Is (vision, for orientation only)

Mahina is a from-scratch Linux-based OS built under a "Documentation-First" methodology (Divine Collection of Knowledge about Luna, `docs/DCKL/`, 7 volumes, 55 accepted Decision Log entries as of `DL-053`). Three Laws govern it: **Own Every Layer** (no distro base), **Local First** (offline-capable, cloud opt-in), **Aesthetic Is Functional** (motion = system state, never decoration). The OS was renamed from "LunaOS" to "Mahina" (DL-045); the in-system AI presence is named "LUNA" and is a separate concept (`luna-ai-d`, not yet started). The intended full stack is: `luna-init` (PID 1) → `luna-splash` (boot graphics) → `lgp-compositor` (custom display protocol, **not** Wayland) → `LunaGUI` (toolkit) → `luna-shell`/`luna-island`/`luna-bar` (desktop) → `luna-ai-d` (AI daemon) → `lpkg` (package manager). This vision section is unchanged from the DCKL and is **not** what's built — see §1.3 for what actually exists.

### 1.2 Subsystem Status (ground truth, not documentation)

| Subsystem | Status | Summary |
|---|---|---|
| **limine bootloader config** | Implemented | `boot/limine.conf` — trivial, 5-line config pointing at `vmlinuz-mahina`/`initramfs-mahina.img`, 5s timeout. Nothing to build; just a config file. |
| **Kernel config** | Implemented (fragment) | `kernel/.config` + `.config.notes` — a `merge_config.sh`-style fragment (not a full `.config`), every `CONFIG_*` justified in `.config.notes`. Covers x86_64/EFI/SMP, Btrfs+ext4, VirtIO, DRM/KMS+fbdev, evdev/HID/USB input, AppArmor, seccomp-BPF, cgroups v2. **Inconsistency found:** `.config.notes` and DL-009 say "Linux 6.6.x LTS," but `scripts/build-kernel.sh` actually downloads and builds **6.9.7**. Not previously flagged in the repo's own audits. |
| **luna-init (PID 1)** | **Implemented** | 20 `.c`/`.h` files, C17, statically linked. Every Stage 0–4 subsystem is real: signalfd+epoll event loop, zombie reaper, fstab.toml mounting, custom TOML parser (fuzz-tested), service loader, Kahn's-algorithm dependency graph + cycle detection, async supervisor (100ms timerfd-driven `supervisor_pump()`, not blocking), cgroup v2 assignment, `setgid`/`setgroups(0,NULL)`/`setuid` privilege drop (correct order), inotify-based live service-file reload, Unix-socket JSON control server, orderly shutdown/reboot, panic→emergency-shell fallback. |
| **luna-splash** | **Implemented** | 4 files. `/dev/fb0` mmap, embedded 8×16 bitmap font, real block-character "MAHINA" logo render (not an ASCII fallback — outdated docs claimed otherwise), progress bar, pipe-based `PERCENT|MSG\n` IPC from `luna-init`, fade-out, and a synchronous handoff (`splash_stop()` waits for the child to fully release `/dev/fb0` before the compositor starts). |
| **luna-init-ctl** | Implemented (basic) | One file. Connects to `/run/luna-init.sock`, sends/receives newline-delimited JSON. Help/usage text exists and is complete. |
| **lgp-compositor** | **Partially implemented (~25%)** | 16 source files across `config/`, `drm/`, `kms/`, `ipc/`, `logging/`, `protocol/`, `scene/`, `util/`. Real DRM/KMS dumb-buffer modesetting (legacy `drmModeSetCrtc`, not atomic — by design for M1), a Unix-socket server at `/run/lgp/compositor.sock` (`0660 root:video`), a 6-byte TLV wire protocol (2-byte type + 4-byte length — see DL-053, a real protocol decision made *after* the code, see §0/§5), `LGP_HELLO` handshake with capability negotiation gated by `SO_PEERCRED` uid, persistent per-connection client objects with proper stream-reassembly receive buffers, and a working surface model: `LGP_CREATE_SURFACE` / `LGP_DESTROY_SURFACE` / `LGP_COMMIT_BUFFER` with `SCM_RIGHTS`-passed shared-memory `XRGB8888` buffers, per-surface-type/layer permission checks, and a real layered compositor (`lgp_surface_manager_composite()` paints a background "LUNA Void" color then blits surfaces in `wallpaper→application→shell→overlay→notification→luna_island→system_modal→cursor` layer order). A standalone `lgp-test-client` binary exercises the full handshake→create→commit flow. |
| **LunaGUI** | Not started | Zero code. Blocked on a stable `lgp-compositor` socket (which now exists in early form) — `IMPLEMENTATION_STATUS.md`'s own "next task" is to build the client library against it. |
| **luna-shell / luna-bar / luna-island** | Not started | Zero code. Documented in DCKL Volume V / Volume VII. |
| **luna-ai-d (LUNA AI daemon)** | Not started | Zero code. Python/asyncio per DL-049; default model Qwen2.5 3B Q4_K_M via Ollama per DL-046. A `services/ollama.toml` and `services/luna-ai-d.toml` exist as **service definitions only** — they declare how the binary *would* be supervised; the binaries don't exist, so these services will fail their "binary exists" pre-fork check if ever started. |
| **lpkg (package manager)** | Not started | Zero code. Ed25519/libsodium signing planned (DL-048). |
| **luna-lock (screen lock)** | Not started | Zero code (DL-035 accepted, no implementation). |
| **AppArmor / seccomp profiles** | Not started | Kernel support is compiled in (`kernel/.config`); no actual profiles committed for any daemon. |
| **Build system (Makefile)** | Implemented | `make all` builds `luna-init` + `luna-init-ctl` + `luna-splash`; `src/lgp-compositor/Makefile.inc` appends `lgp-compositor` + `lgp-test-client` to the same `all` target (Make allows multiple prerequisite-only rules for one target). `make test-unit`, `make test-fuzz`, `make lint`, `make image`, `make run-qemu` all exist and are wired up — see §2.3 for what each one actually covers. |
| **CI (`.github/workflows/ci.yml`)** | Implemented (narrower than it looks) | 4 gates: Build → Unit Tests → clang-tidy → Fuzz regression. **Gap not previously documented:** Gate 1 ("Build") only runs `make luna-init` and `make luna-init-ctl` — it never builds `luna-splash` or `lgp-compositor` in CI, and `lint` (Gate 3) only lints `luna-init`/`luna-init-ctl`/`luna-splash` sources, never `lgp-compositor`. Unit tests (Gate 2) and fuzzing (Gate 4) *do* cover `lgp-compositor`'s protocol/surface code, just not the DRM/KMS/IPC glue or `main.c`. The stale legacy `build.yml` workflow mentioned in three of the four status docs **does not exist** — already deleted. |
| **Test suite** | Minimal but growing | 13 Unity-framework tests total: 8 in `tests/unit/luna-init/` (log, toml ×3, depgraph ×3 incl. a cycle-detection case, service ×1) + 5 new in `tests/unit/lgp-compositor/protocol_test.c`. Plus 2 libFuzzer/AFL++ harnesses (`fuzz_toml.c`, `fuzz_lgp_tlv.c`, the latter new). `supervisor.c`, `mount.c`, `shutdown.c`, `panic.c`, `signal.c`, and all of `lgp-compositor`'s DRM/KMS/IPC code have zero test coverage. |
| **Documentation (DCKL)** | Implemented (comprehensive, partially stale) | 7 volumes, ~60 files, 55 accepted Decision Log entries. Architecture docs for Stage 0 are faithful to the code. Volume III (Graphics) needed a real correction (DL-053) to match the shipped wire format — done. The 5 "living" status docs in `docs/System Prompts/` are where staleness concentrates (§0). |

### 1.3 Boot Flow Reconstruction (from implementation, not docs)

```
UEFI
  │
  ▼
Limine  (boot/limine.conf — loads vmlinuz-mahina + initramfs-mahina.img)
  │
  ▼
Linux Kernel (6.9.7 per build script; kernel/.config targets "6.6.x LTS" per docs — mismatch, see §1.2)
  │
  ▼
Initramfs
  │
  ▼
luna-init  (PID 1, statically linked C17)
  │
  ├─ Stage 0: early /proc /sys /dev mounts (inline, before logging is up)
  ├─ Stage 1: signalfd + epoll event loop init, zombie reaper armed
  ├─ Stage 2: fstab.toml mounts — /dev/pts /tmp /run /dev/shm, cgroup2 at /sys/fs/cgroup
  ├─ Stage 3: hostname (/etc/luna/hostname → "mahinabox"), entropy seed
  ├─ Stage 4: load /etc/luna/services/*.toml → depgraph (Kahn's) → topological
  │            start (dbus, networkmanager, ntpd, pipewire, wireplumber declared;
  │            ollama/luna-ai-d/lgp-compositor declared but not started here —
  │            lgp-compositor is deferred to Stage 5 explicitly)
  │            luna-splash forked as a child, fed PERCENT|MSG\n progress over a pipe
  ├─ [boot-complete check, then 3.5s splash hold]
  ├─ Stage 5: splash_stop() — SIGTERM + synchronous waitpid() for luna-splash to
  │            release /dev/fb0 (incl. its fade-out) — THEN
  │            supervisor_start_one("lgp-compositor") — forks /usr/bin/lgp-compositor,
  │            which opens /dev/dri/card0 directly via raw DRM/KMS (implicit master
  │            acquisition, not explicitly verified — see §5), creates a dumb buffer,
  │            paints it black ("LUNA Void," DL-043), commits the CRTC, opens
  │            /run/lgp/compositor.sock
  │            luna_log_switch_to_runtime() — switches luna-init's own logging
  │            from boot.log to runtime.log
  ├─ Stage 6: NOT GATED — no D-Bus "compositor Ready" wait exists yet (DL-031 is
  │            accepted but unimplemented), so luna-init proceeds unconditionally:
  │            prints welcome banner, forks an interactive shell on stdout AND a
  │            second shell on /dev/tty1. This is the literal current end state of
  │            every boot: two BusyBox/sh shells running side-by-side with the
  │            compositor alive in the background, listening on its socket, with
  │            nothing yet connecting to it except the manual lgp-test-client.
  └─ Stage 7: AI runtime — not reached; luna-ai-d does not exist.
```

This differs from `PROJECT_STATE.md`'s description ("v0.1 boots to Stages 0–4 then drops to shell, Stages 5–7 are empty stubs") in one specific, important way: **Stage 5 is no longer an empty stub.** It runs real code that starts a real compositor process. The shell-drop at the end is still the de facto end state because Stage 6 (shell/LunaGUI) has no client yet to use the compositor for anything.

### 1.4 Repository Inventory

```
MahinaOS/
├── boot/limine.conf                  Trivial bootloader config. Stable, done.
├── kernel/.config(.notes)            Kernel config fragment + per-option rationale.
├── etc/luna/                         Runtime config: hostname, fstab.toml, sysctl.toml,
│                                      services/*.toml (8 service defs — 3 have no
│                                      corresponding binary yet: ollama, luna-ai-d,
│                                      and (until recently) lgp-compositor).
├── src/
│   ├── luna-init/                    20 files. Most mature code in the repo. No dead
│   │                                  files, but depgraph_topo_sort()/dep_indices[]
│   │                                  are computed and unit-tested yet never consumed
│   │                                  by the supervisor (real dead code — §5).
│   ├── luna-init-ctl/                1 file, complete.
│   ├── luna-splash/                  4 files, complete.
│   └── lgp-compositor/               16 files across 7 subdirectories (config, drm,
│                                      kms, ipc, logging, protocol, scene, util) +
│                                      main.c + test_client.c. All real, all building,
│                                      all M1–M3-labeled in comments. Youngest and
│                                      least-tested code in the repo (see §5).
├── tests/
│   ├── unit/luna-init/               4 files, 8 tests. No coverage gaps remain stale
│   │                                  (the old test_toml.c/test_depgraph.c duplicates
│   │                                  flagged by old docs are already gone).
│   ├── unit/lgp-compositor/          1 file (protocol_test.c), 5 tests — new.
│   ├── fuzz/toml/, fuzz/lgp_tlv/     2 libFuzzer harnesses + seed corpora.
│   └── vendor/unity/                 Unity test framework (MIT), vendored, untouched.
├── scripts/                          build-kernel.sh (downloads 6.9.7 — see §1.2),
│                                      build-initramfs.sh, build-image.sh, run-qemu.sh.
├── docs/
│   ├── DCKL/                         7 volumes, ~60 files, 55 accepted DL entries.
│   │                                  Internally consistent; one real correction
│   │                                  needed and made (DL-053 superseding DL-025).
│   ├── Architecture Reviews/          6 AR sessions + 2 sprint reports + bring-up docs.
│   ├── CODE_AUDIT_REPORT.md           Dated 2026-06-29. Mostly resolved by 496451a;
│   ├── ARCHITECTURE_COMPLIANCE_REPORT.md  remaining open items carried into §5 below.
│   ├── test_results_stage0.md         Small (31-line) Stage-0 QEMU boot test log.
│   └── System Prompts/                AI_CONTEXT.md + IMPLEMENTATION_STATUS.md are
│                                       current (updated in HEAD commit); PROJECT_STATE.md,
│                                       ARCHITECTURE_CONSISTENCY_REPORT.md,
│                                       REPOSITORY_HEALTH_REPORT.md, Progress.md are stale.
├── Mahina_DCKL_Complete.md            Monolithic concat of all DCKL docs (~23.8k lines),
│                                       maintained as an AI-context-loading convenience.
├── .github/workflows/ci.yml           Only active workflow (build.yml is already gone).
├── Makefile                           Root build; lgp-compositor wired in via include.
├── ROADMAP.md / CHANGELOG.md          ROADMAP.md is stale ("Phase 0... Current");
│                                       CHANGELOG.md's "Unreleased" section is accurate
│                                       and up to date with the lgp-compositor work.
└── README.md, LICENSE (MIT), AUTHORS.md, CONTRIBUTING.md, etc.   Standard OSS boilerplate.
```

No `LunaOS`-era stale references, no generated/vendored code beyond `tests/vendor/unity` (correctly vendored, not generated), and no unused/orphaned source files were found anywhere in `src/`. The repository is unusually clean for its size — the staleness problem is entirely confined to the five `docs/System Prompts/*.md` living-status files, not to the code or the DCKL itself.

### 1.5 Current Development Phase (from implementation)

- **Current milestone:** Phase 2 — Graphics Layer bring-up (`lgp-compositor`), ~25% complete by the project's own estimate, independently corroborated by the source review in this snapshot.
- **Current sprint, concretely:** moving from "compositor can paint a black screen and shake hands with one test client" to "compositor can actually composite a committed shared-memory surface onto the screen and a second client can be built against the now-frozen wire format."
- **Current blockers:**
  1. No client exists yet other than the hand-written `lgp-test-client` — `LunaGUI` (the real first client) hasn't been started.
  2. No D-Bus implementation anywhere (`org.mahina.compositor.Ready`, DL-031) — Stage 5→6 handoff is unconditional, not gated.
  3. DRM master acquisition in `drm_device_open()` is an unverified assumption, not an explicit check (see §5).
- **Immediate next task** (matches the repo's own `IMPLEMENTATION_STATUS.md` "Next Task" for `lgp-compositor`): boot in QEMU, run `lgp-test-client` against the live compositor, confirm the committed surface actually appears on screen end-to-end, then begin the `LunaGUI` client library against the now-DL-053-frozen wire format.
- **Long-term next task:** input (`libinput`, DL-032) and a second/third real client so the surface-permission model (`LGP_CAP_*`) gets exercised by more than one trusted local process.

### 1.6 Reality Check

**✅ Fully working**
- `luna-init` Stage 0–4 boot, service supervision, dependency resolution, cgroups, privilege drop, control socket.
- `luna-splash` framebuffer rendering and IPC.
- `luna-init`↔`luna-splash` handoff, including the synchronous fade-out wait.
- `lgp-compositor`'s DRM/KMS modesetting to a solid black frame (verified by direct code reading of `kms_crtc_commit`/`kms_dumb_buffer_create`).
- `lgp-compositor`'s socket lifecycle, TLV framing + stream reassembly, `LGP_HELLO` handshake + capability negotiation.
- The TOML parser, dependency graph cycle detection, build system, and CI's unit/fuzz gates (within the narrower scope they actually cover — see §5).

**🟡 Partially working**
- `lgp-compositor` surface creation/commit/composite path — implemented and internally consistent, but only ever exercised against the one hand-written `lgp-test-client`; never proven against an independent client implementation.
- Boot-stage logging — works, but shutdown-path log lines never reach the runtime log file (closed too early — see §5).
- Service readiness checks — `file`/`socket` methods work; `http`/`signal` are explicit, loudly-logged stubs (by design, not silently broken).

**🟠 Prototype**
- `lgp-compositor` as a whole is explicitly M1–M3-labeled prototype code by its own authors' comments — DRM master handling, legacy (non-atomic) modesetting, and the lack of any crash-path DRM-master release are all "good enough for one trusted local process," not production-hardened.
- `lgp-test-client` — a debug/bring-up tool, not a real application.

**🔴 Planned only (zero code)**
- LunaGUI, luna-shell, luna-bar, luna-island, luna-ai-d, lpkg, luna-lock, AppArmor/seccomp profiles, libinput integration, Vulkan backend, atomic KMS.

---

## 2. Implementation Matrix

### 2.1 Feature Matrix

| Feature | Status | Notes |
|---|---|---|
| Bootloader (limine) | ✅ Done | Static config only |
| Kernel config | ✅ Done (fragment) | Version mismatch: docs say 6.6.x, build script uses 6.9.7 |
| luna-init (PID 1) | ✅ Done | All Stage 0–4 subsystems |
| Service Manager | ✅ Done | TOML-driven, async |
| Dependency Graph | 🟡 Partial | `depgraph_build()`+cycle detection used; `depgraph_topo_sort()` computed but dead — supervisor re-derives order ad hoc from `after[]` |
| TOML Parser | ✅ Done | Custom, fuzzed, leniency bugs (bool/int trailing-garbage, array closing-bracket) already fixed |
| Runtime Logging | 🟡 Partial | Switch is called; fd now actually opens (fixed); but `shutdown_run()` closes the log *before* logging shutdown steps, so shutdown-path messages never reach the file |
| Control Socket (`ctl.c`) | ✅ Done | JSON over Unix socket; accept path now non-blocking (fixed) |
| Framebuffer / Splash | ✅ Done | Real block-character logo, fade-out, bpp-safe rendering (fixed) |
| DRM/KMS (lgp-compositor) | 🟡 Partial | Legacy `drmModeSetCrtc` only; atomic capability detected but unused (by design for M1); DRM master acquisition unverified |
| Unix Socket (LGP) | ✅ Done | `0660 root:video`, stale-socket cleanup, `SOCK_NONBLOCK` |
| TLV Protocol | ✅ Done | 6-byte header (2B type + 4B length), now a real Decision Log entry (DL-053) instead of a silent deviation from DL-025 |
| Surface Management | 🟡 Partial | Create/destroy/commit implemented with permission checks; only one client implementation has ever exercised it |
| SHM Buffers | ✅ Done | `SCM_RIGHTS` fd passing + `mmap(PROT_READ, MAP_SHARED)`, size/stride validated against overflow |
| Page Flips | 🟡 Partial | `drmModePageFlip` wired with an event handler; handler currently only logs, doesn't yet drive an animation/vsync loop |
| Input (libinput) | 🔴 Not started | DL-032 accepted, no code |
| Cursor | 🔴 Not started | `LGP_LAYER_CURSOR` exists as a layer constant in the compositor; nothing produces a cursor surface |
| Window Manager | 🔴 Not started | Beyond raw surface create/destroy, no window semantics (focus, move, resize) exist |
| Scene Graph | 🟡 Minimal | `lgp_surface_manager_t` is a flat, fixed-size array composited by layer order — functional, not a graph |
| LunaGUI | 🔴 Not started | Zero code |
| Installer | 🔴 Not started | Documented (`06_installer.md`), no code |
| Package Manager (lpkg) | 🔴 Not started | Zero code |
| AI Runtime (luna-ai-d) | 🔴 Not started | Zero code; service file exists, binary doesn't |
| Luna Island | 🔴 Not started | `LGP_SURFACE_LUNA_ISLAND`/`LGP_CAP_LUNA_ISLAND` exist as protocol-level reservations only |
| Networking | 🟡 Declared only | `networkmanager.toml` service file exists; NetworkManager itself isn't part of this repo |
| Shell (luna-shell) | 🔴 Not started | Zero code. (Interactive BusyBox/sh shells are spawned by `luna-init` itself as a fallback, not `luna-shell`.) |
| SDK | 🔴 Not started | Documented (`08_sdk.md`), no code |
| Package Signing | 🔴 Not started | Ed25519 planned (DL-048), no code |
| Driver Manager | 🔴 Not started | No concept of this exists in code yet |

### 2.2 Per-Module Detail — luna-init (all ✅ unless noted)

| File | Provides |
|---|---|
| `log.c/.h` | `LUNA_INFO/WARN/ERROR/FATAL`, boot+runtime log, `fsync` on FATAL only |
| `signal.c/.h` | signalfd+epoll; SIGCHLD/TERM/INT/HUP/USR1 |
| `reaper.c/.h` | `waitpid(-1, WNOHANG)` zombie reaping |
| `toml.c/.h` | Custom parser, 64KB doc limit, 128-entry cap, fuzzed |
| `service.c/.h` | Full TOML field parsing incl. `[service.env]` (now implemented) and `[service.identity]` |
| `depgraph.c/.h` | 🟡 Cycle detection used; `topo_sort()` dead (see §5) |
| `supervisor.c/.h` | Async state machine, cgroup v2, identity drop w/ `setgroups(0,NULL)` |
| `mount.c/.h` | fstab.toml mounting, critical-vs-warning failure split |
| `hostname.c/.h` | `sethostname()` from `/etc/luna/hostname` |
| `splash.c/.h` | Forks/feeds `luna-splash`; correct fallback path |
| `ctl.c/.h` | JSON control socket; non-blocking accept (fixed) |
| `panic.c/.h` | Emergency shell fallback chain |
| `shutdown.c/.h` | Reverse-order stop→unmount→sync→reboot(2); 🟡 logs to stderr only (log closed too early) |
| `console.c/.h` | Welcome banner + shell drop |
| `main.c` | 7-stage sequence; Stage 5 now real (see §1.3) |

### 2.3 Per-Module Detail — lgp-compositor

| File | Maturity | Notes |
|---|---|---|
| `main.c` (598 lines) | 🟡 Functional prototype | Full event loop, client table, message dispatch for HELLO/FILL_RECT/CREATE_SURFACE/DESTROY_SURFACE/COMMIT_BUFFER |
| `drm/device.c`, `drm/connector.c` | 🟡 Functional, unverified master acquisition | Opens `/dev/dri/card0`, detects atomic-capable but doesn't use it |
| `kms/crtc.c`, `kms/dumb_buffer.c`, `kms/page_flip.c` | ✅ Functional for M1 scope | Legacy modesetting, dumb-buffer allocation, page-flip request + event drain |
| `ipc/socket_server.c`, `ipc/client.c` | ✅ Functional | Correct permissions, `SO_PEERCRED` identity, persistent client lifecycle |
| `protocol/tlv.c/.h` | ✅ Functional | 6-byte header, now DL-053-canonical |
| `protocol/hello.c/.h` | ✅ Functional | Version check, caps negotiation, gates all other message types behind handshake |
| `protocol/caps.c/.h` | 🟡 Partial | uid-0 gating for privileged caps is real; caps aren't re-checked on every subsequent message type (e.g. `FILL_RECT` bypasses caps entirely) |
| `protocol/surface.c/.h` | ✅ Functional | Encode/decode for create/destroy/commit payloads |
| `scene/surface.c/.h` (320 lines) | ✅ Functional | Validation, fixed-size surface table, layer-ordered compositing, per-client cleanup on disconnect |
| `config/args.c/.h` | ✅ Functional | `-h`/`-v` only |
| `logging/log.c/.h` | ✅ Functional | Mirrors `luna-init`'s logger design |
| `util/signal.c/.h` | ✅ Functional | signalfd for SIGTERM/SIGINT |
| `test_client.c` (257 lines) | ✅ Functional debug tool | Full HELLO→CREATE→COMMIT exercise client, expanded substantially in the latest commit |

### 2.4 Decision Log Summary (55 entries, DL-001–DL-053)

All entries are `ACCEPTED` except `DL-025`, which is `SUPERSEDED by DL-053` (the LGP wire format was implemented as 2-byte-type+4-byte-length and the Decision Log was corrected to match, rather than the code being changed to match the old decision — see §5 for why this matters going forward). `DL-P04` (license) is closed, superseded by `DL-052` (MIT). No other accepted decision was found contradicted by code; gaps (D-Bus readiness signal DL-031, libinput DL-032) are correctly "not yet built," not "built wrong."

### 2.5 Test Coverage Matrix

| Module | Tests | Quality |
|---|---|---|
| `log.c` | 1 | Happy path only |
| `toml.c` | 3 | Includes malformed-input cases |
| `depgraph.c` | 3 | Includes a cycle-detection case |
| `service.c` | 1 | `service_find()` only |
| `lgp-compositor` protocol/surface | 5 | New; covers TLV + surface encode/decode |
| `supervisor.c`, `mount.c`, `shutdown.c`, `panic.c`, `signal.c`, `hostname.c`, `console.c` | 0 | No coverage |
| `lgp-compositor` DRM/KMS/IPC/main.c | 0 | No coverage (not buildable/runnable without real DRM hardware or a KVM/QEMU GPU passthrough, which likely explains the gap) |
| TOML fuzzer | libFuzzer/AFL++ | Good — mutation coverage of the parser |
| LGP TLV fuzzer | libFuzzer/AFL++ | New — one seed corpus file (`create_surface.bin`) |

---

## 3. Development Timeline

Reconstructed from `git log --reverse` (35 commits, 2026-06-26 → 2026-06-30):

```
2026-06-26  Phase 0 — Documentation-first architecture begins
            bbf3b42 Initial commit
            af05317 → 0727157  System prompts + first DCKL volumes drafted
                       (init system, scheduler, memory, security, filesystem,
                        networking, logging, IPC chapters; decision log seeded)
              │
2026-06-27  DCKL completed
            50e1524 → d569235  Remaining DCKL volumes (LGP, AI architecture,
                       component ownership, implementation roadmap) +
                       FIRST CODE: kernel config, luna-init logging skeleton,
                       build system, bootloader config
              │
2026-06-28  Stage 0 — Boot infrastructure
            da54b71  GitHub Actions CI added; procfs mount bug fixed
            ad829e1  Stage 0 boot completes: kernel graphics fixes, BusyBox shell
            7012def  Open-source boilerplate + first luna-splash implementation
            277276a, 20da2c4  Repo-structure/doc polish
            34333af  luna-init: service identity enforcement (setuid/setgid),
                       splash fallback path fixed
                       [PROJECT_STATE.md / AI_CONTEXT.md last meaningfully
                        updated around here — this is the staleness baseline]
            4f463ad  Negative-path unit tests added (toml errors, depgraph cycles)
            e740f77  inotify service reload + runtime log switch wired in
            f62ecbe  Supervisor made async (timerfd-driven); cgroups v2 added
              │
2026-06-28→29  luna-splash hardening
            a29d3b9  Architecture consistency report (now itself stale)
            2ec1989  Splash graphics alignment/sync/terminal-restore fixes
            729bc2c  Stage 0 test results documented
            10e8c30  Service supervisor finalized; LGP COMPOSITOR SKELETON BEGINS
            dbfef0c  Rootfs mount/logging fixes; splash fade-out implemented
            d0ff87f  QEMU GUI display enabled
            a4fa29b  Splash fade-out lag fixed
              │
2026-06-29  Self-audit + Phase 2 (Graphics) bring-up
            7d02b43  CODE_AUDIT_REPORT.md + ARCHITECTURE_COMPLIANCE_REPORT.md
                       written — catches stale docs, wire-format violation,
                       client-lifecycle bug, runtime-log bug, and more
            496451a  fix(audit): addresses essentially every finding above
                       in one large commit across decision_log.md, 01_lgp.md,
                       lgp-compositor/main.c, and 8 luna-init files
            ba29e17  Framebuffer graphics + init console refinements
              │
2026-06-30  Current HEAD — LGP surface model
            591e280  Implement LGP surface commits: CREATE/DESTROY/COMMIT_BUFFER,
                       scene/surface.c compositor, SCM_RIGHTS shared memory,
                       expanded test_client.c, new protocol/surface unit tests,
                       new LGP TLV fuzz target, AI_CONTEXT.md +
                       IMPLEMENTATION_STATUS.md regenerated to match
                       ── YOU ARE HERE ──
```

In four days the project went from "documentation only" to a booting, service-supervised init system with a graphical splash and an early but real display-compositor protocol — and, notably, ran a genuine internal audit-and-fix cycle (`7d02b43`→`496451a`) before continuing, which is why most of the bugs an external reviewer would normally find were already caught and fixed by the time this snapshot was taken.

---

## 4. Next Engineering Steps

**Immediate next task** (lowest-risk, highest-value, matches what the code itself is set up for):
1. Boot the current tree in QEMU and run `lgp-test-client` end-to-end against the live `lgp-compositor` to visually confirm a committed shared-memory surface actually appears on screen, not just that the protocol handshake completes. The Makefile and DRM/KMS/IPC code already support this; it just hasn't been confirmed as a full visual round-trip in this snapshot's review (the audit and this document worked from source reading, not a live boot).
2. Fix the two narrowly-scoped, low-risk items that are pure correctness fixes with no design implications (good candidates to bundle with #1): re-derive `supervisor_pump()`'s start order from `depgraph_topo_sort()` instead of maintaining two parallel implementations, and move `luna_log_close()` in `shutdown_run()` to *after* the stop/unmount steps so shutdown-path log lines aren't lost.

**Near-term (already has dependencies in place):**
- Add `lgp-compositor` (and `luna-splash`) to the CI build/lint gates — right now CI only compiles and lints `luna-init`/`luna-init-ctl`, so a broken `lgp-compositor` build could land on `main` without CI catching it. The `Makefile.inc` plumbing already exists; this is a one-line CI change, not new engineering.
- Begin the `LunaGUI` client library against the now-frozen (DL-053) wire format — this is explicitly the next listed task in `IMPLEMENTATION_STATUS.md` and has a real socket + protocol to build against for the first time.
- Reconcile the kernel version: either update `scripts/build-kernel.sh` to 6.6.x to match DL-009/`.config.notes`, or supersede DL-009 with a new decision documenting 6.9.7 — same class of fix as the DL-025→DL-053 precedent already set in this repo.
- Regenerate `PROJECT_STATE.md`, `ARCHITECTURE_CONSISTENCY_REPORT.md`, and `REPOSITORY_HEALTH_REPORT.md` (or retire them in favor of `IMPLEMENTATION_STATUS.md` + this document) so the next contributor or AI agent doesn't act on the stale claims identified in §0.

**Long-term (real dependencies, but blocked on the above landing first):**
- `libinput` integration (DL-032) — needs a stable compositor event loop, which exists, but should follow a confirmed visual round-trip (#1 above) rather than precede it.
- A second independent LGP client (beyond `LunaGUI` itself, e.g. a second test harness) to actually exercise the per-connection capability model now that it's enforced at handshake time — right now only one trusted local process has ever talked to the socket.
- D-Bus `org.mahina.compositor.Ready` (DL-031) so Stage 5→6 becomes a real gate instead of an unconditional fall-through.

**What should absolutely NOT be started yet** (per repository state, not per opinion):
- `luna-shell`, `luna-island`, `luna-bar` — these are LunaGUI clients; starting them before LunaGUI exists would mean building against a toolkit that doesn't exist yet.
- `luna-ai-d` — its service file already exists for supervision purposes, but starting the actual daemon work has no UI to surface into and no compositor client surface to render a presence indicator on yet.
- `lpkg` — no installed-system concept exists yet (no installer, no real root filesystem layout beyond the initramfs); a package manager has nothing to manage.
- Atomic KMS / Vulkan backend — explicitly sequenced after the legacy/software path in DL-026 and not yet warranted at ~25% compositor completion.
- Any change to the LGP wire format, the TOML parser's public API, the CTL socket JSON protocol, or the boot stage sequence — these are explicitly called out in `AI_CONTEXT.md` as things that must not change without a new Decision Log entry, and nothing in the current codebase state creates pressure to revisit any of them.

---

## 5. Technical Debt

Severity reflects current `HEAD` (`591e280`), not the historical audit — most Critical/High findings from `docs/CODE_AUDIT_REPORT.md` were already fixed in `496451a`. Items below are either confirmed still-open from that audit, or newly identified during this snapshot's independent review.

### 🟠 High
- **`protocol/caps.c` — capability enforcement is real for surfaces, but not universal.** `LGP_CAP_LAYER_SHELL`/`LGP_CAP_LUNA_ISLAND`/`LGP_CAP_CANVAS_SURFACE` are correctly checked in `lgp_surface_validate_request()`, but the legacy `LGP_MSG_FILL_RECT` message handler (`main.c::lgp_handle_fill_rect`) performs no capability check at all — any handshaken client can repaint the entire screen via `FILL_RECT` regardless of granted caps. Low real-world risk today (one trusted local test client), but worth closing before a second, less-trusted client exists.
- **CI does not build or lint `lgp-compositor` or `luna-splash`.** `.github/workflows/ci.yml` Gate 1 runs only `make luna-init` and `make luna-init-ctl`; Gate 3 (`lint`) only covers `LUNA_INIT_SOURCES`/`LUNA_CTL_SOURCES`/`LUNA_SPLASH_SOURCES`. `lgp-compositor`'s `main.c`, `drm/*`, `kms/*`, `ipc/*`, and `config/args.c` are exercised only indirectly via the unit-test/fuzz gates (which compile `tlv.c`/`surface.c`/`log.c` only) — a broken build of the actual `lgp-compositor` binary would not be caught by CI today. Not previously documented in the repo's own audits.

### 🟡 Medium
- **DRM master acquisition is an unverified assumption** (`drm/device.c::drm_device_open()`): no explicit `drmSetMaster()` call or `EBUSY`/`EACCES` handling; relies implicitly on `luna-splash` having released `/dev/fb0` first. Works in the current single-process bring-up environment; flagged by the repo's own audit and still true at `HEAD`.
- **`depgraph_topo_sort()` and `dep_indices[]` are dead code.** Computed, unit-tested (3 tests), never called outside `depgraph.c`/its own test. `supervisor_pump()` independently re-derives start order from `svc->after[]` every pump cycle. The two happen to agree today; this is duplicated logic, not a bug, but it's real layering debt.
- **`shutdown_run()` closes the log before logging the shutdown sequence itself.** `luna_log_close()` runs as "Step 1," before `supervisor_stop_one()`/`mount_unmount_all()`, both of which log — those messages go to stderr only, never to the persistent log, which is exactly the period where postmortem debugging matters most for an init system.
- **Kernel version mismatch:** DL-009/`.config.notes` specify Linux 6.6.x LTS; `scripts/build-kernel.sh` builds 6.9.7. Newly identified in this snapshot, not in the repo's prior audits.
- **`[service.env]` is now parsed** (fixed since the prior audit), but no shipped `etc/luna/services/*.toml` file uses it yet, so the path remains untested against a real service.

### 🟢 Minor
- `drm/device.c`'s detected `atomic_supported` flag is set but never branched on (`kms/crtc.c` always uses legacy `drmModeSetCrtc`) — intentional placeholder for future atomic-KMS work per the code's own comments, not a bug.
- `kms/page_flip.c`'s page-flip-complete handler only logs (`LGP_DEBUG`) — it doesn't yet drive an animation/vsync loop, which is fine since nothing animates yet.
- No `atexit`/crash handler releases DRM master if `lgp-compositor` dies via an unhandled signal (e.g. `SIGSEGV`); the kernel reclaims the fd on process exit regardless, so this is a low-severity gap, not a leak.
- `toml_get()` returns the first matching entry for a duplicate key rather than the last (no real `.toml` file in the repo currently duplicates a key, so this is latent).
- `[service.depends].before` is parsed into graph edges but no shipped service file uses it, so that path is also latent/untested.
- `parse_signal_name()` silently defaults to `SIGTERM` for an unrecognized `stop.signal` string (e.g. a typo) instead of logging it as invalid — low risk since SIGTERM is a safe default, but inconsistent with the project's own "no silent failures" standard.
- Five of the repository's own "living" status documents (`PROJECT_STATE.md`, `ARCHITECTURE_CONSISTENCY_REPORT.md`, `REPOSITORY_HEALTH_REPORT.md`, `Progress.md`, and root `ROADMAP.md`) are stale relative to code and should be regenerated or retired — process debt, not code debt, but real enough to have already caused confusion once (per `ARCHITECTURE_COMPLIANCE_REPORT.md` §1).

### ✅ Resolved since the last internal audit (`7d02b43` → `496451a` / `591e280`) — listed for traceability, not action
TLV wire format vs. DL-025 (resolved via DL-053); `lgp-compositor` client destroyed immediately after handshake; no per-client stream-reassembly buffer; runtime log fd never opened; `setuid`/`setgid` without `setgroups(0,NULL)`; `supervisor_start_one()` not propagating spawn failure; `render_logo()`/`render_progress()` assuming 32bpp; TOML boolean/integer trailing-garbage leniency; `ctl.c` accept path blocking PID 1's event loop; `[service.env]` documented-but-unparsed; ctl.yml legacy `build.yml` duplicate; stale `tests/unit/test_toml.c`/`test_depgraph.c` duplicates; `DL-P04` license entry left pending.

---

*This document supersedes `PROJECT_STATE.md` as the authoritative state snapshot until that file is regenerated. It was produced by direct repository inspection at commit `591e280`, not by trusting any prior status document.*
