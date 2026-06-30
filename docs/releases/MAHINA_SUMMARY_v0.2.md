# Mahina OS — Engineering Summary
**The canonical answer to "What is Mahina today?"**
Generated from repository state at commit `5418b47` (2026-06-30). Where documentation and code disagreed, code won.

---

## 1. Project Overview

Mahina OS is a ground-up Linux-based operating system: custom kernel configuration, a custom PID 1 init system (`luna-init`), a custom display protocol (LGP) and display server (`lgp-compositor`), a native widget toolkit (`LunaGUI`), a dedicated Window Manager (`luna-shell`), and a local-first AI presence (LUNA) planned as a layer on top. It does not base on Arch, Debian, or any upstream distro — every component is either written from scratch or deliberately chosen and understood.

**Philosophy** rests on three non-negotiable laws: *Own Every Layer* (no borrowed base, no mystery components), *Local First, Cloud When Useful* (the system must work fully offline; cloud AI is opt-in only), and *Aesthetic Is Functional* (visual polish is treated as information density, not decoration).

**Long-term vision**: an OS that feels like a collaborator rather than a tool — a dark cyberpunk, anime-influenced aesthetic combined with an AI presence (LUNA) that observes and assists without being asked. The project is documentation-first: architecture is written into the DCKL (Divine Collection of Knowledge about Luna) before code is allowed to exist.

---

## 2. Current Development State

- **Current Phase:** Phase 3 (AI & Shell Integration), underway. Phase 0 (Core Foundation), Phase 1 (Boot Aesthetics), and Phase 2 (Graphics Layer / LunaGUI / Userland) are complete and verified.
- **Current Focus:** Integrating local AI models, package management, and system hardening.
- **Most recent work session (2026-06-30):** Full stabilization sprint resolving compilation issues, adding missing build rules, repairing the font rendering engine with canvas clipping, extending IPC capability checks (`LGP_CAP_CLIPBOARD` and `LGP_CAP_WINDOW_MANAGER`), and deploying all ten native applications into the system image.
- **Version label:** `0.2.0 (Waxing)` per the project's moon-phase versioning scheme.
- **Not yet started:** AI runtime (`luna-ai-d`) and package manager (`lpkg`).

---

## 3. What's Already Built

### luna-init — PID 1 init system
**Status:** Complete (~95%). **Purpose:** custom init replacing systemd/OpenRC/s6.
Handles signal handling via signalfd+epoll, zombie reaping, a custom C17 TOML parser (fuzz-tested), a service dependency graph with cycle detection (Kahn's algorithm), an async service supervisor with cgroup v2 assignment, mount management, hostname setup, a panic handler with emergency shell fallback, graceful shutdown, a JSON control socket, and inotify-based live service reload. Two readiness modes (`READY_HTTP`, `READY_SIGNAL`) are stubbed and always return true.

### luna-splash — boot graphics engine
**Status:** Complete (100%). **Purpose:** zero-malloc framebuffer boot splash.
Maps `/dev/fb0` directly, renders an embedded 8×16 bitmap font, progress bars, and a block-character logo, and communicates boot progress from `luna-init` over a pipe.

### lgp-compositor — display compositor
**Status:** Complete (100% Phase 2). **Purpose:** display server using DRM/KMS.
Provides KMS/DRM modesetting, a software rendering engine with Z-order alpha blending, evdev keyboard/mouse input, dynamic key grabs, a clipboard buffer, and secure handshake verification (`LGP_CAP_WINDOW_MANAGER`, `LGP_CAP_CLIPBOARD`, `LGP_CAP_DIRECT_LGP`).

### LunaGUI — application toolkit
**Status:** Complete (100% Phase 2). **Purpose:** the widget/rendering library every userland app is built on.
Features an application loop with LGP IPC, window surface memfd wrapper, canvas clipping stack, PSF1 font engine, Label/Button/Box layouts, scroll container, canvas widget, pointer hittest routing, and keyboard event routing to focused widgets.

### luna-shell — Window Manager & Desktop Shell
**Status:** Complete (100% Phase 2). **Purpose:** the graphical shell environment.
Provides a background gradient wallpaper, top status bar, cascading window manager layout, and global key grabs (`Super+T` terminal launch, `Alt+Tab` application focus cycling).

### Build system
**Status:** Complete (100%). Clang/LLVM + Makefile toolchain. `make all` builds all 15 binaries; `make test-unit` runs ASan+UBSan unit tests; `make test-fuzz` runs libFuzzer/AFL++ regression; `make lint` runs clang-tidy; `make image`/`make run-qemu` build and boot a disk image (Linux/WSL2 only).

### Userland applications (10 native apps deployed)
- **luna-terminal**: VT100/ANSI terminal with circular scrollback, PTY resize, and paste integration.
- **luna-installer**: 10-page wizard with simulated install progress.
- **luna-settings**: 6 pages (Display, Network, Audio, Users, About).
- **luna-files**: POSIX directory browser.
- **luna-calc**: 4-function calculator.
- **luna-text**: Text file viewer.
- **luna-about**: CPU/RAM/Host system information reader.
- **luna-tasks**: `/proc`-based task supervisor.

---

## 4. Current Boot Flow

1. **Firmware/Kernel handoff** — Limine bootloader hands off to the Linux kernel, which execs `luna-init` as PID 1.
2. **Stage 1** — `luna-init` initializes its epoll event loop and signal handling.
3. **Stage 2** — Mounts filesystems (`/proc`, `/sys`, `/dev`, `/tmp`, `/run`, `/sys/fs/cgroup`) per `fstab.toml`.
4. **Stage 3** — Sets hostname from `/etc/luna/hostname` (falls back to `"mahina"`).
5. **Stage 4** — Loads service definitions from `/etc/luna/services/*.toml` and starts services in resolved dependency order.
6. **Stage 5** — Release boot splash, start `lgp-compositor` display server, and launch `luna-shell` window manager.
7. **Stage 6 (Current Boot End)** — Fully interactive desktop environment loaded (Alt+Tab, wallpaper, top bar, pointer/keyboard routing).

---

## 5. Feature Status

| Feature | Status | Notes |
|---|---|---|
| Bootloader (Limine) | ✅ Complete | Configured via `boot/limine.conf`. |
| Kernel configuration | ✅ Complete (fragment) | Custom CONFIG fragment merged for kernel builds. |
| luna-init (PID 1) | ✅ Complete (~95%) | Custom TOML supervisor. |
| luna-splash | ✅ Complete | Zero-malloc framebuffer progress renderer. |
| Graphics / DRM/KMS | ✅ Complete | Basic modesetting and Z-order ARGB8888 blending. |
| LGP (protocol) | ✅ Complete | TLV framing, capability negotiation, and WM extensions. |
| Surface system | ✅ Complete | memfd + SCM_RIGHTS buffer commits. |
| LunaGUI (toolkit) | ✅ Complete | Full recursive layout tree with inputs and rendering. |
| Window management | ✅ Complete | Privileged WM client (`luna-shell`) cascades and focuses windows. |
| Input (pointer) | ✅ Complete | evdev motion/button events routed to widget hit-testing. |
| Input (keyboard) | ✅ Complete | evdev key events routed to focused surface/widget with global WM grabs. |
| Runtime / AI (luna-ai-d) | 🔴 Not started | Qwen2.5 3B local LLM runtime planned. |
| Package manager (lpkg) | 🔴 Not started | Custom atomic packager planned. |
| Installer (luna-installer) | 🟡 UI complete, backend missing | 10-page flow works; no real disk write. |
| Shell / desktop (luna-shell) | ✅ Complete | Desktop shell with wallpaper, top bar, and hotkeys. |
| Terminal (luna-terminal) | ✅ Complete | ANSI VT100 parser over PTY with scrollback and PTY resize. |
| Core apps | ✅ Complete | Settings, files, calc, text, about, tasks. |
| Security (AppArmor/seccomp) | 🟠 Prototype | Kernel support enabled; daemon profiles pending in Phase 4. |
| Build system | ✅ Complete | Full Makefile toolchain with ASan/UBSan, tidy, and fuzzing. |
| Documentation (DCKL) | ✅ Complete (~92%) | Authority for all system components. |
