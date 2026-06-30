# Mahina OS — Engineering Summary
**The canonical answer to "What is Mahina today?"**
Generated from repository state at commit `87af5953` (2026-06-30). Where documentation and code disagreed, code won.

---

## 1. Project Overview

Mahina OS is a ground-up Linux-based operating system: custom kernel configuration, a custom PID 1 init system (`luna-init`), and a custom display protocol (LGP), with a local-first AI presence (LUNA) planned as a layer on top. It does not base on Arch, Debian, or any upstream distro — every component is either written from scratch or deliberately chosen and understood.

**Philosophy** rests on three non-negotiable laws: *Own Every Layer* (no borrowed base, no mystery components), *Local First, Cloud When Useful* (the system must work fully offline; cloud AI is opt-in only), and *Aesthetic Is Functional* (visual polish is treated as information density, not decoration).

**Long-term vision**: an OS that feels like a collaborator rather than a tool — a dark cyberpunk, anime-influenced aesthetic combined with an AI presence (LUNA) that observes and assists without being asked. The project is documentation-first: architecture is written into the DCKL (Document Control Knowledge Library) before code is allowed to exist.

---

## 2. Current Development State

- **Current Phase:** Phase 2 (Graphics Layer / LunaGUI / Userland), well underway. Phase 0 (Core Foundation) and Phase 1 (Boot Aesthetics) are complete.
- **Current Focus:** finishing the LGP compositor's window-management and input pipeline, expanding the LunaGUI toolkit, and standing up the first wave of userland applications (which is now largely done — see §3).
- **Most recent work session (2026-06-30):** a single large engineering sprint that fixed compositor security/clamping bugs, rewrote the LunaGUI renderer with a real design system, shipped a full 10-page graphical installer UI, implemented an ANSI/VT100 terminal parser, and implemented all six planned core desktop applications.
- **Version label:** `0.1.0 (Waxing)` per the project's moon-phase versioning scheme.
- **Not yet started:** AI runtime (`luna-ai-d`), package manager (`lpkg`), and the full desktop shell/window manager beyond a skeleton.

---

## 3. What's Already Built

### luna-init — PID 1 init system
**Status:** Complete (~95%). **Purpose:** custom init replacing systemd/OpenRC/s6. **Maturity:** production-shaped, lightly tested.
Handles signal handling via signalfd+epoll, zombie reaping, a custom C17 TOML parser (fuzz-tested), a service dependency graph with cycle detection (Kahn's algorithm), an async service supervisor with cgroup v2 assignment, mount management, hostname setup, a panic handler with emergency shell fallback, graceful shutdown, a JSON control socket, and inotify-based live service reload. Two readiness modes (`READY_HTTP`, `READY_SIGNAL`) are stubbed and always return true.

### luna-splash — boot graphics engine
**Status:** Complete (100%). **Purpose:** zero-malloc framebuffer boot splash. **Maturity:** production-shaped.
Maps `/dev/fb0` directly, renders an embedded 8×16 bitmap font, progress bars, and a block-character logo, and communicates boot progress from `luna-init` over a pipe. Compositor handoff is currently a one-frame black cut by design (deferred until the compositor owns display).

### luna-init-ctl — control CLI
**Status:** Complete, basic (~80%). **Purpose:** CLI for talking to `luna-init`'s control socket (list/status/start/stop/shutdown/reboot as JSON). No shell completion yet.

### lgp-compositor — Luna Graphics Protocol compositor
**Status:** In progress (~25%). **Purpose:** the only display server in Mahina — a from-scratch, Wayland-inspired but much simpler protocol over DRM/KMS.
Has basic KMS/DRM modesetting, a minimal software renderer, a Unix socket server with 6-byte TLV wire framing, the `LGP_HELLO` handshake, surface lifecycle (`CREATE_SURFACE`/`DESTROY_SURFACE`/`COMMIT_BUFFER`), shared-memory buffer commits via `SCM_RIGHTS`, Z-ordering, ARGB8888 software alpha blending, and basic mouse input with capability-gated direct pixel fills. No window management, no libinput backend, no D-Bus readiness signal, no Vulkan backend yet.

### LunaGUI — application toolkit
**Status:** In progress (~60%). **Purpose:** the widget/rendering library every userland app is built on.
Application loop with LGP IPC and HELLO handshake; window/surface wrapper using memfd + `SCM_RIGHTS`; canvas primitives (fill rect, PSF bitmap text, outlined rects); Label, Button, VBox, and HBox widgets; recursive widget-tree destruction (leak-safe page navigation); full pointer input routing (motion + button → hit-test → click callback) as of the 2026-06-30 sprint; and a real "Mahina Design System" color palette/renderer. No theme engine, animation engine, image loading, keyboard input, text-input widget, or scroll container yet — keyboard input specifically is blocked on compositor-side keyboard event routing.

### Build system
**Status:** Complete (100%). Clang/LLVM + Makefile toolchain. `make all` builds all 15 binaries; `make test-unit` runs ASan+UBSan unit tests; `make test-fuzz` runs libFuzzer/AFL++ regression; `make lint` runs clang-tidy; `make image`/`make run-qemu` build and boot a disk image (Linux/WSL2 only).

### Tests
**Status:** Present but thin. 7 test source files: 4 `luna-init` unit suites (log, TOML, dependency graph, service lookup — 38 assertions total), 1 `lgp-compositor` protocol test (35 assertions), and 2 fuzz targets (TOML parser, LGP TLV framing). 11 `luna-init` modules and all of `luna-splash` have zero test coverage. No QEMU boot test exists in CI.

### Documentation (DCKL)
**Status:** Complete and extensive (~92%). Seven volumes covering foundation, architecture, graphics/presence, AI/presence, userland/SDK, a development bible, and an implementation roadmap, plus a 53-entry accepted decision log. This is the largest single artifact in the repository by a wide margin (`Mahina_DCKL_Complete.md` alone is ~24,000 lines).

### Kernel configuration
**Status:** Complete as a config fragment (90%). x86_64/EFI/SMP, Btrfs/ext4, VirtIO (for QEMU), DRM/KMS+fbdev, input (evdev/HID/USB), networking (TCP/IP, nftables), AppArmor (enforcing default), seccomp-BPF, and audio (HDA/virtio-sound) are all enabled. This is a `merge_config.sh` fragment, not a standalone `.config` — it must be merged onto a real Linux 6.6.x defconfig to build a kernel. AppArmor and seccomp are enabled at the kernel level only; no actual profiles or filters exist in any daemon yet.

### Userland applications (Priority 7 — all six shipped 2026-06-30)
**Status:** Complete. luna-settings (6 pages: Display, Network, Audio, Users, About), luna-files (POSIX directory browser via opendir/readdir), luna-calc (4-function calculator), luna-text (file viewer, up to 2000 lines, paged scroll), luna-about (live `/proc` system info), luna-tasks (process list from `/proc/<pid>/status`, SIGTERM kill). All are LunaGUI clients; all free their widget trees on every page transition.

### luna-installer
**Status:** Complete UI, 0% backend. A real 10-page graphical flow (Welcome → Language → Keyboard → Timezone → Disk Selection → Partitioning Summary → User Creation → Summary → Installation Progress → Complete/Reboot). Installation progress is simulated; there is no actual `mkfs`/partitioning/debootstrap code behind it.

### luna-terminal
**Status:** In progress (~70%). A real ANSI/VT100 escape-sequence state machine (GROUND→ESCAPE→CSI→CSI_PARAM), per-cell fg/bg/bold tracking, 16-color SGR palette, cursor movement and erase sequences, a 2000-line scrollback buffer, and PTY spawn/resize via `forkpty`/`TIOCSWINSZ`. Rendering is text-only (no pixel-accurate cell grid yet); there's no copy/paste, no tabs, and no keyboard input path into the shell (blocked on the same compositor keyboard gap as LunaGUI).

### luna-desktop (shell/dock/wallpaper)
**Status:** Skeleton (~25%). A `main.c` exists under `src/luna-desktop/` but the shell, status bar, and "Luna Island" conversation panel described in the DCKL are not implemented beyond this scaffold.

### Not started at all (zero source code exists)
`luna-ai-d` (AI runtime/presence daemon — only a `luna-ai-d.toml` service stub exists, no implementation), `lpkg` (package manager — no code anywhere in the repository).

---

## 4. Current Boot Flow

This is the actual sequence as implemented in `src/luna-init/main.c`, not the planned five-phase architecture from the DCKL:

1. **Firmware/Kernel handoff** — Limine bootloader hands off to the Linux kernel, which execs `luna-init` as PID 1.
2. **Stage 1** — `luna-init` initializes its epoll event loop and signal handling (signalfd for SIGCHLD/TERM/INT/HUP/USR1).
3. **Stage 2** — Mounts filesystems (`/proc`, `/sys`, `/dev`, `/tmp`, `/run`, `/sys/fs/cgroup`) per `fstab.toml`.
4. **Stage 3** — Runs early hooks: sets hostname from `/etc/luna/hostname` (falls back to `"mahina"`), other early setup.
5. **Stage 4** — Loads service definitions from `/etc/luna/services/*.toml`, builds the dependency graph, starts services in resolved order under the async supervisor (cgroup v2 assignment, restart policy, readiness checks).
6. **Stage 0 (v0.1) boot complete** — the log explicitly states *"Stage 0 (v0.1) boot complete. Stages 5-7 pending."* This is the actual current end state of a real boot.
7. **Stage 5 (present but minimal)** — releases the boot splash and starts `lgp-compositor`. Beyond this point (shell, AI, full desktop), the code contains intentional stubs only — the source comment for `main.c` itself states *"Stage 5+: Skeletons only (graphics/shell/AI are v0.5+)."*

In practice: a Mahina boot today gets you from firmware to a supervised service set with the compositor launching, but does not yet reach a finished interactive desktop session — that requires the still-in-progress compositor window management, keyboard input routing, and `luna-desktop` shell.

---

## 5. Feature Status

| Feature | Status | Notes |
|---|---|---|
| Bootloader (Limine) | ✅ Complete | Configured via `boot/limine.conf`. |
| Kernel configuration | ✅ Complete (fragment) | Merge fragment, not standalone `.config`. |
| luna-init (PID 1) | ✅ Complete (~95%) | Two readiness modes stubbed. |
| luna-splash | ✅ Complete | Compositor handoff is a known 1-frame cut by design. |
| Graphics / DRM/KMS | 🔵 In progress | Basic modesetting and software renderer only; no Vulkan backend. |
| LGP (protocol) | 🔵 In progress (~25%) | TLV framing, HELLO handshake, surface commit basics done. |
| Surface system | 🔵 In progress | Create/destroy/commit, Z-order, ARGB8888 blending implemented. |
| LunaGUI (toolkit) | 🔵 In progress (~60%) | Core widgets + pointer input routing done; no keyboard, theming, or animation. |
| Window management | 🔴 Planned | No code exists in the compositor. |
| Scene graph | 🟠 Prototype | Compositor-owned surface list with Z-order; not a general scene graph. |
| Input (pointer) | 🟡 Working | Motion + button routed end-to-end to widget hit-testing. |
| Input (keyboard) | 🔴 Planned | No compositor-side keyboard event routing exists yet. |
| Runtime / AI (luna-ai-d) | 🔴 Not started | Only a service TOML stub; zero implementation source. |
| Package manager (lpkg) | 🔴 Not started | Zero code anywhere in the repository. |
| Installer (luna-installer) | 🟡 UI complete, backend missing | 10-page flow works; no real disk write. |
| Shell / desktop (luna-desktop) | 🟠 Prototype (~25%) | Single skeleton `main.c`. |
| Terminal (luna-terminal) | 🔵 In progress (~70%) | Real ANSI parser + PTY; text-only rendering, no keyboard input. |
| Core apps (settings/files/calc/text/about/tasks) | ✅ Complete | All six implemented and functional as of 2026-06-30. |
| Networking | 🟠 Prototype | Kernel support + NetworkManager service config present; no custom networking code. |
| Security (AppArmor/seccomp) | 🟠 Prototype | Enabled at kernel-config level only; no profiles/filters in any daemon yet. |
| Build system | ✅ Complete | Full Makefile toolchain, CI gates for build/test/lint/fuzz. |
| Tests | 🟠 Prototype | 7 test files, very low coverage breadth (11+ modules untested). |
| Documentation (DCKL) | ✅ Complete (~92%) | Far ahead of implementation; some volumes describe unbuilt subsystems. |

---

## 6. Repository Structure

- **`src/`** — all implementation code. Contains `luna-init/` (init, complete), `luna-init-ctl/` (control CLI, complete), `luna-splash/` (boot splash, complete), `lgp-compositor/` (compositor, in progress, organized into `drm/`, `kms/`, `protocol/`, `scene/`, `ipc/`, `input/`, `config/`, `logging/`, `util/` subdirs), `luna-gui/` (toolkit, in progress, split into `core/`, `render/`, `widgets/`, `include/`), and one directory per userland app (`luna-settings`, `luna-files`, `luna-calc`, `luna-text`, `luna-about`, `luna-tasks`, `luna-installer`, `luna-terminal`, `luna-desktop`). 98 C/H files, ~11,600 lines.
- **`kernel/`** — `.config` fragment and notes for the target Linux 6.6.x kernel. Not a buildable standalone config.
- **`boot/`** — Limine bootloader configuration.
- **`etc/luna/`** — runtime TOML configuration: service definitions (`services/*.toml`), `fstab.toml`, `sysctl.toml`, `hostname`. This is what `luna-init` actually parses at boot.
- **`scripts/`** — shell scripts for building the kernel, initramfs, disk image, and running QEMU.
- **`tests/`** — `unit/` (per-module C tests using the bundled Unity framework), `fuzz/` (libFuzzer targets for TOML and LGP TLV parsing), `vendor/unity/` (vendored test framework).
- **`docs/`** — the DCKL (seven volumes), architecture review session notes, audit/compliance reports, and "System Prompts" (living status docs: `IMPLEMENTATION_STATUS.md`, `PROJECT_STATE.md`, `AI_CONTEXT.md`, etc.). This is the largest and most mature part of the repository by content volume — documentation substantially outpaces code.
- **`Mahina_DCKL_Complete.md`** — a single concatenated ~24,000-line export of the entire DCKL at repo root.
- **`MAHINA_ENGINEERING_SNAPSHOT.md`** — a prior point-in-time engineering snapshot at repo root (predates this document).
- **Top-level meta files** — `README.md` (currently stale — still describes "Phase 1: Boot Aesthetics" as current, which implementation has moved well past), `ROADMAP.md` (high-level, also lags actual progress), `CHANGELOG.md` (accurate and current as of the last commit), `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, `LICENSE` (MIT).

---

## 7. Current Technical Debt

**High**
- No keyboard input path anywhere in the stack (compositor → LunaGUI → terminal/apps). This blocks the terminal, any text-input widget, and meaningful interactive use of the desktop.
- `luna-installer` has no real backend — it cannot actually install the OS to disk. The entire installation flow is currently a simulation.
- Security is effectively unenforced at runtime: AppArmor and seccomp are kernel-enabled but no profiles or filters exist for any daemon, despite this being called out as a hard requirement (DL-020) for the third-party sandbox.
- Test coverage is very thin relative to surface area: `supervisor.c`, `mount.c`, `shutdown.c`, `panic.c`, `signal.c`, `hostname.c`, `console.c`, `ctl.c`, `splash.c`, and all of `luna-splash`'s render/IPC code have zero unit tests.

**Medium**
- `lgp-compositor` has no window management, no libinput backend (mouse handling is currently hand-rolled, not via libinput), and no D-Bus readiness signal — all documented as accepted dependencies that aren't wired up yet.
- LunaGUI has no theme engine (colors are hardcoded in `render.c`), no animation engine, and no image loading — meaning no PNG/SVG widget exists for any app that needs one.
- `luna-terminal` renders text-only rather than through a real pixel cell grid; full rendering needs `lgui_window_get_canvas()` integration that hasn't been done.
- `README.md` and `ROADMAP.md` describe the project as being in "Phase 1: Boot Aesthetics," which is no longer accurate — implementation has moved well into Phase 2/3 territory by the actual code. These should be refreshed from `IMPLEMENTATION_STATUS.md`/this document.

**Low**
- Two literal `TODO` comments remain in source: dispatching `LGP_MSG_POINTER_BUTTON` for left/right clicks distinctly in `mouse.c`, and storing the language selection on click in the installer's language page.
- A stale legacy `build.yml` CI workflow exists alongside the active `ci.yml` and should be deleted.
- DL-P04 (license decision) is still marked "Pending" in the decision log despite MIT already being the license in use.
- Kernel config notes don't have an explicit `CONFIG_CGROUP_V2` entry, even though it's implied on modern kernels via `CONFIG_CGROUPS`.

---

## 8. Current Roadmap

**What should be built next, and why:**
1. **Compositor keyboard input.** This single gap blocks the terminal, text widgets, and any meaningfully interactive app. It is the highest-leverage next piece of work because LunaGUI's pointer-input plumbing already exists as a template for how keyboard events should be routed.
2. **`lgp-compositor` window management.** Multiple surfaces currently composite with Z-order but without real window semantics (focus, move, resize). This is a prerequisite for `luna-desktop` to become more than a skeleton.
3. **`luna-desktop` shell.** Now that all six core apps and the toolkit exist, building a real dock/launcher/wallpaper shell around them is unblocked and would make the existing app set actually usable as a desktop.
4. **Installer backend.** The UI is finished; wiring it to real `mkfs`/partition/copy operations is a contained, well-scoped piece of work with no remaining design ambiguity.

**What should not be started yet:**
- `luna-ai-d` (AI runtime) — the DCKL marks this Phase 3, and it has hard dependencies (D-Bus interface, context engine, presence engine) that assume a working shell and compositor exist first. Starting it now would mean building against a moving, incomplete substrate.
- `lpkg` (package manager) — needs a stable userland and signing/trust model decisions that, while documented (DL-048, DL-019, DL-020), have no implementation precedent yet in this codebase to build from.
- Vulkan rendering backend in the compositor — explicitly sequenced in the DCKL to come after the software renderer is solid, which it currently is not.

**Dependencies already in place:** the LGP wire protocol (TLV framing, HELLO handshake) is stable enough to build on; the LunaGUI widget tree and destroy/rebuild pattern is proven across six real apps; the service supervisor in `luna-init` is mature enough to host new daemons (AI runtime, future package manager) once they exist, since service TOML stubs for some of these already exist in `etc/luna/services/`.

---

## 9. Current Reality

**✅ Complete**
luna-init · luna-splash · luna-init-ctl · build system · luna-settings · luna-files · luna-calc · luna-text · luna-about · luna-tasks · luna-installer (UI only) · DCKL documentation

**🟡 In Progress**
LunaGUI toolkit · luna-terminal · luna-shell/luna-desktop components beyond the bare skeleton

**🟠 Prototype**
lgp-compositor · luna-desktop (skeleton) · networking (kernel + config only) · security enforcement (kernel flags only, no profiles) · scene graph (Z-ordered surface list, not general-purpose) · test suite (present but shallow)

**🔴 Planned**
luna-ai-d · lpkg · window management · keyboard input (full stack) · theme engine · animation engine · Vulkan backend · AppArmor/seccomp profiles

---

## 10. Development Timeline

- **2026-06-26 — Documentation genesis.** Repository starts as a pure documentation project under the working name "LunaOS." Foundation, architecture, security, filesystem, networking, logging, and IPC chapters written before any code.
- **2026-06-27 — DCKL completed; first code scaffolding.** The full seven-volume DCKL, decision log, and implementation roadmap are written out. The project is renamed Mahina OS in this window. Initial kernel config, `luna-init` logging, build system, and bootloader setup land the same day documentation closes out — strictly following the project's documentation-first rule.
- **2026-06-28 — Phase 0/1 build-out.** Stage 0 boot completed in QEMU with kernel graphics fixes and a busybox fallback shell. `luna-splash` implemented. Service identity enforcement, async supervisor, cgroups, and inotify-based reload added to `luna-init`. CI (GitHub Actions) introduced.
- **2026-06-29 — Audit and graphics bring-up begins.** A formal code audit and architecture compliance report are produced and immediately acted on (all findings fixed same day). Framebuffer console work continues; `lgp-compositor` scaffolding starts.
- **2026-06-30 — Phase 2/3 sprint.** LGP surface commit path, mouse/capability fixes, full LunaGUI input routing and renderer rewrite, the complete 10-page installer UI, the ANSI/VT100 terminal, and all six core desktop applications are implemented in a single autonomous engineering session. `IMPLEMENTATION_STATUS.md` and `CHANGELOG.md` updated to reflect the new state — which is the snapshot this document is built from.

---

## 11. Current Statistics

- **Total commits:** 41
- **Repository size:** ~3.8 MB working tree (~1.4 MB `.git`)
- **Source files:** 98 (58 `.c`, 40 `.h`), ~11,600 lines of C across `src/`, `kernel/`, `boot/`
- **Documentation files:** 90 Markdown files under `docs/`, plus 2 large root-level documents (`Mahina_DCKL_Complete.md` at ~23,800 lines, `MAHINA_ENGINEERING_SNAPSHOT.md`)
- **Tests:** 7 test source files (4 `luna-init` unit suites, 1 `lgp-compositor` protocol suite, 2 fuzz targets) covering 4 of roughly 15 testable modules
- **Current implementation phase:** Phase 2 (Graphics Layer / LunaGUI / Userland), per `IMPLEMENTATION_STATUS.md`'s own dashboard
- **Current architecture version:** DL-053 is the highest accepted decision log entry (supersedes DL-025 for the LGP wire format)
- **Current DCKL status:** all seven volumes populated and marked canonical/complete, with one open item (DL-P04 license decision still flagged "Pending" despite MIT being in active use)

---

## 12. Executive Summary

A new engineer joining Mahina today should know this: the boot chain, init system, and boot splash are solid and essentially done; the display protocol and compositor exist and can create and composite surfaces but have no window management or keyboard input yet; the LunaGUI toolkit and six core desktop apps were built in a single dense sprint on 2026-06-30 and work end-to-end for mouse-driven interaction; the installer UI and terminal are functionally complete but each is missing one critical piece (disk-write backend, keyboard input, respectively); and the AI runtime and package manager — both extensively designed in the DCKL — have no code at all and should not be started until the compositor's keyboard pipeline and `luna-desktop` shell exist to host them. Documentation is far ahead of implementation everywhere except `README.md` and `ROADMAP.md`, which are stale; when those two disagree with `IMPLEMENTATION_STATUS.md`, `CHANGELOG.md`, or the code itself, trust the latter three. The single highest-leverage next task is wiring keyboard events through the compositor into LunaGUI, since it unblocks the terminal, text input, and real desktop interactivity all at once.
