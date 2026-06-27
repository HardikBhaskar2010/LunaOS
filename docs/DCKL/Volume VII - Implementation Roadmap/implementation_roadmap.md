# Mahina — Implementation Roadmap
**Volume VII · The Build Checklist**
**Classification:** Living Document — Updated as items are completed
**Status:** Active · This is the daily work order

---

## Purpose

This document answers one question: **what gets built first?**

It is a sequential checklist. Each item has a clear definition of "done." Nothing in a later section begins until the items before it are checked off. This is the daily engineering reference for the next year.

Read this document before writing a single line of code.
Update it the moment a milestone is complete.
Never skip an item because it seems small — small items are load-bearing.

---

```
Legend:
  [ ] Not started
  [/] In progress
  [x] Complete
  [~] Deferred (with reason noted)
  [!] Blocked (on decision or dependency)
```

---

## STAGE 0 — Bare Metal

> **Exit criterion:** The machine boots, mounts the root filesystem, and gives an interactive root shell. Nothing graphical. Nothing networked. Just: alive.

### 0.1 — Development Environment

- [ ] Host toolchain: cross-compiler for x86_64-linux-gnu (GCC or Clang)
- [ ] Security Tooling: AddressSanitizer and UndefinedBehaviorSanitizer integrated for Debug builds
- [ ] Static Analysis: `clang-tidy` integration established in CI pipeline
- [ ] QEMU/KVM virtual machine configured for Mahina development
- [ ] KVM machine profile: 4 cores, 4 GB RAM, virtio disk, virtio-vga
- [ ] Build system chosen (Makefile, Meson, or custom) and scaffolded
- [ ] Git repository initialized and remote pushed (`https://github.com/HardikBhaskar2010/Mahina.git`)
- [ ] Directory layout decided: `src/`, `boot/`, `docs/`, `packages/`, `scripts/`

**Done when:** `git clone` + `make` produces a bootable image.

---

### 0.2 — Bootloader

- [ ] limine version pinned
- [ ] limine EFI binary placed at `/boot/efi/EFI/BOOT/BOOTX64.EFI` (DL-012)
- [ ] `limine.cfg` written with Mahina kernel entry
- [ ] Boot entry: kernel path, initramfs path, kernel command line
- [ ] Kernel command line baseline: `root=/dev/... rw quiet`
- [ ] EFI System Partition: FAT32, mounted at `/boot/efi`
- [ ] QEMU boots from EFI and loads the kernel image

**Done when:** QEMU reaches the kernel panic screen (because there is no init yet). The bootloader worked.

---

### 0.3 — Kernel Configuration

- [ ] Upstream Linux kernel version pinned (LTS release)
- [ ] `mahina_defconfig` created — start from `x86_64_defconfig`
- [ ] Enable: cgroup v2 (CONFIG_CGROUPS, CONFIG_CGROUP_V2)
- [ ] Enable: BPF (CONFIG_BPF, CONFIG_BPF_SYSCALL)
- [ ] Enable: io_uring (CONFIG_IO_URING)
- [ ] Enable: DRM/KMS (CONFIG_DRM, CONFIG_DRM_KMS_HELPER)
- [ ] Enable: DRM for target GPU (virtio-vga for QEMU, then Intel/AMD later)
- [ ] Enable: devtmpfs (CONFIG_DEVTMPFS, CONFIG_DEVTMPFS_MOUNT)
- [ ] Enable: tmpfs (CONFIG_TMPFS)
- [ ] Enable: Btrfs (CONFIG_BTRFS_FS) (DL-027)
- [ ] Enable: AppArmor (CONFIG_SECURITY_APPARMOR)
- [ ] Enable: seccomp (CONFIG_SECCOMP, CONFIG_SECCOMP_FILTER)
- [ ] Enable: POSIX message queues (CONFIG_POSIX_MQUEUE)
- [ ] Enable: Unix domain sockets (CONFIG_UNIX)
- [ ] Enable: netfilter/nftables (CONFIG_NF_TABLES)
- [ ] Enable: USB HID (for keyboard/mouse — QEMU input)
- [ ] Enable: virtio block, virtio net (QEMU devices)
- [ ] Disable: systemd dependencies (CONFIG_GENTOO_LINUX_INIT_SYSTEMD off)
- [ ] Kernel builds cleanly with `mahina_defconfig`

**Done when:** Kernel image (`vmlinuz-mahina`) is under 10MB compressed and boots in QEMU.

---

### 0.4 — Root Filesystem Layout

- [ ] Disk image created: partition table (GPT), EFI partition, root partition
- [ ] Root filesystem formatted (Btrfs, per DL-027)
- [ ] Standard directories created: `/bin`, `/sbin`, `/usr`, `/lib`, `/etc`, `/var`, `/run`, `/tmp`, `/proc`, `/sys`, `/dev`, `/boot`
- [ ] Symlinks created: `/bin → /usr/bin`, `/sbin → /usr/sbin`, `/lib → /usr/lib` (if /usr merge adopted)
- [ ] `/etc/fstab` written: root, /boot/efi, /tmp (tmpfs), /run (tmpfs)
- [ ] `/etc/luna/` directory created (system config root)
- [ ] `/var/log/luna-init/` directory created (boot log location)
- [ ] `/var/lib/lpkg/` directory created (package database location)

**Done when:** Filesystem mounts cleanly and directory structure matches `Volume II / 09_filesystem.md`.

---

### 0.5 — C Runtime (libc)

- [ ] glibc version pinned (v1 — musl migration is v2 per DL-007)
- [ ] glibc compiled for Mahina target
- [ ] Dynamic linker path configured: `/lib/ld-linux-x86-64.so.2`
- [ ] Basic POSIX test: hello world C program compiles, links, and runs on target

**Done when:** A statically-linked and dynamically-linked C binary both run in QEMU.

---

### 0.6 — luna-init: PID 1

- [ ] `luna-init` repository/subdirectory created: `src/luna-init/`
- [ ] Language: C, single file to start, split later
- [ ] Stage 0: kernel mounts (proc, sysfs, devtmpfs) — implemented
- [ ] Stage 1: root filesystem pivot (from initramfs to real root) — implemented
- [ ] Stage 2: core device nodes created via devtmpfs
- [ ] Stage 3: hostname set, `/etc/hostname` read
- [ ] Logging: boot log writes to `/var/log/luna-init/boot.log` (plaintext, append)
- [ ] Log format: `[luna-init] [LEVEL] message` (from `Volume II / 11_logging.md`)
- [ ] Signal handling: SIGCHLD (reap children), SIGTERM (shutdown), SIGINT (reboot)
- [ ] luna-init reaches a shell (busybox ash or bash) at Stage 3 exit

**Done when:** QEMU shows the Mahina boot log and drops to an interactive root shell.

---

### 0.7 — initramfs

- [ ] initramfs build script written
- [ ] Includes: luna-init binary, busybox, udev (or mdev), kernel modules for root disk
- [ ] initramfs mounts real root and execve's `/sbin/luna-init` from real root
- [ ] Boot sequence works: UEFI → limine → kernel → initramfs → real root → luna-init shell

**Done when:** Full boot chain works in QEMU. Stage 0 complete.

---

## STAGE 1 — System Foundation

> **Exit criterion:** The system has networking, a package manager, and can install software without graphical output.

### 1.1 — luna-init: Stage 4 Services

- [ ] Service definition format specified: TOML in `/etc/luna/services/` (DL-008)
- [ ] Service TOML schema implemented: `name`, `binary`, `args`, `slice`, `restart`
- [ ] luna-init service runner: reads service files, forks+execs services in their cgroup slice
- [ ] Cgroup v2 hierarchy created by luna-init:
  - [ ] `luna-compositor.slice` (weight 800)
  - [ ] `luna-shell.slice` (weight 400)
  - [ ] `luna-session.slice` (weight 300)
  - [ ] `luna-ai.slice` (weight 200)
  - [ ] `luna-system.slice` (weight 300)
- [ ] Service: `nftables` — starts before NetworkManager, applies `/etc/nftables.conf`
- [ ] Service: `D-Bus` daemon — starts, socket available at `/run/dbus/system_bus_socket`
- [ ] Service: `NetworkManager` — starts, connects to D-Bus, manages `eth0` in QEMU
- [ ] Service: `ntpd` or `chrony` — starts, syncs clock (DL-015)
- [ ] Service: `PipeWire` — starts (audio, not needed for Stage 1 but service file present)
- [ ] DHCP works: QEMU guest gets an IP address from QEMU's NAT network
- [ ] `ping 8.8.8.8` works from the root shell

**Done when:** Network is up. `curl https://example.com` works from the root shell.

---

### 1.2 — Logging Infrastructure

- [ ] Boot log: `/var/log/luna-init/boot.log` — written by luna-init during boot (already done in 0.6)
- [ ] Runtime log: `/var/log/luna-init/runtime.log` — opened by luna-init after Stage 4, services write here
- [ ] Log rotation: maximum file size defined (e.g., 10 MB), rotation creates `.1`, `.2` suffixes
- [ ] Log severity levels: DEBUG, INFO, WARN, ERROR, FATAL — all implemented in luna-init's log API
- [ ] Service log API: a C header (`luna_log.h`) that services can include to write to the runtime log
- [ ] No syslog. No journald. File-based only. (`Volume II / 11_logging.md`)

**Done when:** Boot log and runtime log exist after every boot. Rotation triggers at 10 MB.

---

### 1.3 — IPC (D-Bus)

- [ ] D-Bus system bus is running (service started in 1.1)
- [ ] D-Bus session bus is running for user sessions
- [ ] Test: `dbus-send` from root shell succeeds
- [ ] IPC patterns documented in `Volume II / 08_ipc.md` are verified against actual D-Bus behavior
- [ ] D-Bus policy file written: `/etc/dbus-1/system.conf` allows luna system services

**Done when:** Two separate processes can communicate via D-Bus in QEMU.

---

### 1.4 — lpkg: Package Manager (Phase 1 — Terminal Only)

- [ ] `lpkg` repository/subdirectory: `src/lpkg/`
- [ ] Language: C
- [ ] Package format decided: tarball with a `PKGINFO` metadata file (TOML)
- [ ] Package metadata schema: `name`, `version`, `arch`, `deps`, `files[]`, `sha256`
- [ ] `lpkg install <file.lpkg>` — installs to `/usr/` (system) or `~/.local/` (user, DL-017)
- [ ] `lpkg remove <name>` — removes installed package files
- [ ] `lpkg list` — lists installed packages from the package database
- [ ] Package database: SQLite at `/var/lib/lpkg/installed.db` (system) or `~/.local/share/lpkg/installed.db` (user)
- [ ] SHA256 integrity check on every install
- [ ] `lpkg install` is atomic: either all files are installed or none (DL-018)
- [ ] No network downloads yet — installs from local `.lpkg` files only

**Done when:** `lpkg install hello-1.0.lpkg` installs a test package. `lpkg remove hello` uninstalls it cleanly. Database reflects both operations.

---

### 1.5 — lpkg: Package Repository (First-Party)

- [ ] Repository format decided: an index file (TOML) + individual `.lpkg` files over HTTPS
- [ ] Test repository hosted (GitHub Releases or a static file server)
- [ ] `lpkg update` — fetches the repository index
- [ ] `lpkg install <name>` — resolves package from index, downloads, verifies SHA256, installs
- [ ] Dependency resolution: if package A depends on B, install B first
- [ ] Signature verification: GPG or ed25519 (DL-019) — sign all first-party packages
- [ ] `lpkg upgrade` — upgrades all installed packages to latest version
- [ ] Pre-update snapshot (DL-011) — Btrfs snapshot before major upgrades (if filesystem permits)

**Done when:** `lpkg install ncurses` downloads and installs ncurses from the first-party repository.

---

## STAGE 2 — Graphics Primitive

> **Exit criterion:** A rectangle appears on screen, drawn by the LGP compositor. No widgets. No shell. Just: a pixel on screen from a compositor-owned surface.

### 2.1 — DRM/KMS Device Setup

- [ ] Open `/dev/dri/card0` in the compositor process
- [ ] Enumerate KMS connectors (virtual display in QEMU)
- [ ] Select active connector + CRTC + mode (1920×1080 or whatever QEMU exposes)
- [ ] Set display mode via `drmModeSetCrtc()`
- [ ] Allocate a framebuffer (GBM buffer or dumb buffer for initial testing)
- [ ] Fill framebuffer with a solid color (LUNA Void #1A1A2E)
- [ ] Submit framebuffer to KMS via page flip
- [ ] QEMU window shows the solid color

**Done when:** QEMU shows a solid LUNA Void background. DRM is working.

---

### 2.2 — GPU Abstraction Layer (lgp-render)

- [ ] `src/lgp-render/` created
- [ ] API designed: `render_target_create()`, `render_clear()`, `render_texture_blit()`, `render_present()`
- [ ] Backend 1: Software renderer (CPU blit to dumb framebuffer) — implemented first
- [ ] Backend 2: OpenGL/EGL via GBM — skeleton only in Stage 2, full in Stage 3
- [ ] lgp-render backend is selected at startup based on hardware capability
- [ ] Test: lgp-render draws a colored rectangle to a GBM framebuffer and presents via KMS

**Done when:** lgp-render's software backend produces a rectangle on screen via KMS.

---

### 2.3 — LGP Compositor: Surface Manager (Minimum)

- [ ] `src/lgp-compositor/` created
- [ ] LGP socket created at `/run/lgp/compositor.sock`
- [ ] Client connections accepted (each client gets a session ID)
- [ ] Message: `LGP_CREATE_SURFACE` — allocate a surface entry
- [ ] Message: `LGP_DESTROY_SURFACE` — free the surface entry
- [ ] Message: `LGP_COMMIT_BUFFER` — accept shm buffer via `SCM_RIGHTS`, map it
- [ ] Compositor frame loop: composite all committed surfaces → present via KMS
- [ ] Compositor signals readiness: write sentinel or D-Bus signal
- [ ] luna-init Stage 5: starts lgp-compositor, waits for readiness signal

**Done when:** A test client connects, creates a surface, commits an shm buffer, and the compositor displays it on screen.

---

### 2.4 — Boot Splash (luna-splash)

- [ ] `src/luna-splash/` created (minimal C binary, no dynamic allocation)
- [ ] Boot splash rendered by `luna-splash` (started in Stage 3)
- [ ] Splash: LUNA Void background + Mahina wordmark (bitmap, no font rendering needed)
- [ ] Splash displays from Stage 3 until compositor starts in Stage 5
- [ ] Transition: `luna-init` kills `luna-splash` (SIGTERM), then starts `lgp-compositor` which claims the framebuffer
- [ ] Boot splash respects Animation Budget: transition ≤ 300ms

**Done when:** Boot shows the splash via `luna-splash`. Compositor start replaces it cleanly.

---

### 2.5 — Input: libinput

- [ ] libinput linked into the compositor
- [ ] libinput seat opened: `/dev/input/` event devices
- [ ] Keyboard events read from libinput and logged (terminal debug output)
- [ ] Pointer (mouse) events read and logged
- [ ] Input events stored in per-frame queue, processed before scene assembly

**Done when:** Moving the QEMU mouse and pressing keys produces log output in the compositor.

---

## STAGE 3 — Living Desktop

> **Exit criterion:** A real desktop with a shell, a status bar, LUNA presence visible, and interactive windows. First time Mahina looks like an operating system.

### 3.1 — LGP Protocol: Full Implementation

- [ ] LGP wire format decided (DL entry required first — TLV binary)
- [ ] All protocol messages implemented:
  - [ ] `LGP_CREATE_SURFACE` with surface type
  - [ ] `LGP_DESTROY_SURFACE`
  - [ ] `LGP_COMMIT_BUFFER`
  - [ ] `LGP_REQUEST_FRAME` / `LGP_FRAME_CALLBACK`
  - [ ] `LGP_SET_SEMANTIC_COLOR`
  - [ ] `LGP_SEND_MOTION`
  - [ ] `LGP_SET_GEOMETRY`
  - [ ] `LGP_SET_LAYER`
  - [ ] `LGP_INPUT_EVENT` (compositor → client)
  - [ ] `LGP_COMPOSITOR_ERROR`
- [ ] Color Resolver: semantic code → theme hex value
- [ ] Motion class validation: reject unknown motion classes
- [ ] Z-order layer system: 7 layers (WALLPAPER through CURSOR)
- [ ] Input routing: pointer hit-test, keyboard focus tracking
- [ ] Frame callbacks: vsync-aligned delivery via DRM vblank
- [ ] GPU backend: EGL/OpenGL or Vulkan (replace software renderer)

**Done when:** The compositor passes a protocol compliance test: all message types work, invalid messages return errors.

---

### 3.2 — Animation Engine

- [ ] Animation engine integrated into compositor render loop
- [ ] All 9 motion classes implemented with correct easing functions
- [ ] Animation Budget enforcement: auto-complete animations that exceed their ceiling
- [ ] Concurrent complexity budget: 30 (default profile)
- [ ] `lgp_animation_t` data structure and state machine implemented
- [ ] WARN log on budget exceeded
- [ ] Test: a surface transitions with `Fade`, `Slide`, `Scale`, `Ripple`, `Expand`, `Collapse` — all visually correct and within budget

**Done when:** All 9 motion classes produce correct on-screen animations that respect budget ceilings.

---

### 3.3 — LunaGUI Toolkit

- [ ] `src/lunagui/` created (C library)
- [ ] Widget tree: `LunaWindow`, `LunaPanel`, `LunaLabel`, `LunaButton`, `LunaToggle`, `LunaTextInput`
- [ ] Box model layout engine: horizontal + vertical containers with grow/shrink
- [ ] Canvas API: `fill_rect`, `stroke_rect`, `fill_text`, `push_clip`, `pop_clip`
- [ ] Semantic color API: `LunaColor.Healthy` → `LGP_SET_SEMANTIC_COLOR(LUNA_GREEN)`
- [ ] Animation API: `widget.animate(LunaAnimation.FadeIn)` → `LGP_SEND_MOTION(Fade)`
- [ ] Event system: `on_click`, `on_change`, `on_submit`, `on_close_request`
- [ ] Text rendering: FreeType + HarfBuzz integration
- [ ] Default font loaded from theme (or hardcoded Inter initially)
- [ ] LunaGUI test application: window with a button and a label — runs on the compositor

**Done when:** A LunaGUI application compiles, connects to the compositor, displays a window, and receives click events.

---

### 3.4 — luna-shell

- [ ] `src/luna-shell/` created (C, uses LunaGUI)
- [ ] Connects to LGP compositor at startup
- [ ] Requests `LAYER_WALLPAPER` surface (desktop background)
- [ ] Renders LUNA Void background
- [ ] Wallpaper image support (PNG decode + LunaCanvas blit) — optional for Stage 3
- [ ] Application launcher: press a key or click to open app list
- [ ] Window management: track open application windows, bring to front on click
- [ ] Workspace: at minimum, a single workspace

**Done when:** luna-shell is the first graphical surface visible after boot. The desktop background appears.

---

### 3.5 — luna-bar

- [ ] `src/luna-bar/` created (C, uses LunaGUI)
- [ ] Requests `LAYER_SHELL` surface
- [ ] Displays: clock (top-right), network status indicator (LUNA Green/Void), AI status dot
- [ ] Clock updates every second
- [ ] Network status: green (connected), void (disconnected) — reads NetworkManager D-Bus
- [ ] luna-bar appears on top of luna-shell desktop

**Done when:** The status bar is visible with a working clock.

---

### 3.6 — luna-island (Static — No Live2D)

- [ ] `src/luna-island/` created (C, uses LunaGUI)
- [ ] Requests `LUNA_ISLAND` surface type at `LAYER_LUNA_ISLAND`
- [ ] Initial appearance: a small dark rounded rectangle in the top-center of the screen
- [ ] Contains: LUNA presence indicator (a color dot — LUNA Green = active, LUNA Void = idle)
- [ ] luna-island receives mode changes from luna-ai-d via D-Bus
- [ ] Mode change animates: `Fade` out current state, `Fade` in new state
- [ ] luna-island is always visible — never minimized, never covered (Z-order enforces)

**Done when:** Luna Island is visible on the desktop. The presence indicator dot changes color when LUNA's mode changes.

---

### 3.7 — luna-ai-d: Presence Engine (No LLM)

- [ ] `src/luna-ai-d/` created (C)
- [ ] Component split: `presence/` and `inference/` subdirectories
- [ ] Presence Engine starts at Stage 6 of boot
- [ ] Context engine: reads `~/.luna/config/observe.toml`, monitors allowed applications
- [ ] Mode detection: maps application context to LUNA mode (AMBIENT, DEVSHELL, FOCUS, STUDY, CREATIVE)
- [ ] D-Bus API: `org.mahina.luna.GetMode()`, `org.mahina.luna.ModeChanged` signal
- [ ] luna-island subscribes to `ModeChanged` signal
- [ ] Memory: observation written to `~/.luna/memory/workflow.db` (SQLite)
- [ ] Inference Engine: NOT started — lazy-load path wired up but Ollama not yet integrated

**Done when:** LUNA changes mode based on what application is in focus. Luna Island reflects the mode change.

---

### 3.8 — luna-notif

- [ ] `src/luna-notif/` created (C, uses LunaGUI)
- [ ] Requests `LAYER_NOTIFICATION` surface
- [ ] D-Bus API: `org.freedesktop.Notifications.Notify` (standard interface)
- [ ] Notification card: title, body, icon — appears with `Slide` animation
- [ ] Notification auto-dismiss after configurable timeout (default 5 seconds)
- [ ] Dismiss animates: `Slide` out
- [ ] Multiple notifications stack vertically

**Done when:** `notify-send "Hello" "World"` produces a notification card on screen.

---

### 3.9 — Luna Dark Theme (Hardcoded)

- [ ] Color Resolver initialized with hardcoded Luna Dark hex values
- [ ] All 5 semantic color codes map to correct hex values
- [ ] Background colors available via compositor context
- [ ] LunaGUI `LunaTheme.current()` returns hardcoded Luna Dark values
- [ ] Typography scale hardcoded: size_md = 15px, Inter (or fallback)

**Done when:** The entire desktop uses the Luna Dark color palette consistently. No hex values are hardcoded in application code — all via semantic tokens.

---

## STAGE 4 — Full Presence

> **Exit criterion:** LUNA can converse, remember, and reason. Package management is complete. The system is self-updating and self-describing.

### 4.1 — luna-ai-d: LLM Inference Engine

- [ ] Ollama binary included in the OS image (via lpkg)
- [ ] Ollama starts lazily: luna-ai-d spawns Ollama subprocess on first LLM request
- [ ] Default model selected and bundled or downloaded at first boot (DL-021)
- [ ] LLM conversation API: D-Bus method `org.mahina.luna.Chat(prompt) → response`
- [ ] Streaming response support: D-Bus signal per token
- [ ] Ollama assigned to `luna-ai.slice` cgroup (DL-021)
- [ ] OOM behavior: Ollama has highest OOM score (killed first under memory pressure)

**Done when:** `luna chat "Hello"` receives a response from the local LLM.

---

### 4.2 — Memory Engine

- [ ] `~/.luna/memory/` directory structure initialized at first luna-ai-d start
- [ ] SQLite databases created: `workflow.db`, `preference.db`, `context_graph.db`
- [ ] Observation pipeline: Presence Engine writes patterns to `workflow.db`
- [ ] Memory security: `chmod 700 ~/.luna/memory/`
- [ ] `luna memory --clear` command: wipes all memory, reinitializes databases
- [ ] `luna memory --audit` command: human-readable dump of stored patterns
- [ ] Shutdown summarization: luna-ai-d produces `persistent_summary.enc` on SIGTERM
- [ ] Encryption: persistent_summary.enc encrypted with user key (DL-023)

**Done when:** After a reboot, LUNA remembers the previous session's context summary.

---

### 4.3 — Theme Engine: Full TOML Loading

- [ ] Theme TOML parser written (uses toml-c or similar)
- [ ] Theme schema validated against required fields
- [ ] Theme directory scanning: `/usr/share/luna/themes/` and `~/.luna/themes/`
- [ ] `~/.luna/config/theme.toml` specifies active theme by name
- [ ] Theme switch: new theme loaded, Color Resolver updated, `LGP_THEME_CHANGED` broadcast
- [ ] LunaGUI handles `LGP_THEME_CHANGED`: re-reads `LunaTheme.current()`, re-layouts
- [ ] Theme picker UI: LunaGUI application showing available themes with previews
- [ ] Test: switch from Luna Dark to a community theme — entire desktop updates without restart

**Done when:** Theme switching works live. No application restart required.

---

### 4.4 — lpkg: Graphical Permission Interface

- [ ] lpkg daemon sends permission request via D-Bus to luna-ai-d
- [ ] luna-ai-d triggers LUNA permission dialog (a `LAYER_SYSTEM_MODAL` compositor surface)
- [ ] Permission dialog: package name, what it installs, trust level — user clicks Allow/Deny
- [ ] Deny: install aborted cleanly
- [ ] Allow: install proceeds
- [ ] Dialog uses LUNA Amber (#FFB800) for warning-level install, LUNA Pink for critical-level

**Done when:** `lpkg install <package>` triggers the graphical permission dialog.

---

### 4.5 — DMA-BUF Support (CANVAS_SURFACE with GPU)

- [ ] DMA-BUF buffer import in compositor: `SCM_RIGHTS` for DMA-BUF fd
- [ ] Compositor imports DMA-BUF as Vulkan external image (or EGL image)
- [ ] CANVAS_SURFACE type: accepts DMA-BUF commits
- [ ] Test: a direct-LGP test client renders a spinning triangle using Vulkan → presents via CANVAS_SURFACE

**Done when:** GPU-rendered content appears on screen without CPU copying.

---

## STAGE 5 — Polish & Completeness

> **Exit criterion:** Mahina is a complete v1 operating system. A real user could daily-drive it.

### 5.1 — Luna Performance Lab

- [ ] Standalone application (`src/luna-perf-lab/`, uses LunaGUI)
- [ ] Subsystem panels: CPU, GPU, Scheduler, Memory, Storage, Networking, AI Runtime, Animation
- [ ] Each parameter: label, current value, explanation, impact indicators
- [ ] Profiles: 🌿 Calm, ⚡ Focus, 🎮 Gaming, 🎥 Creative, 🚀 Unleashed
- [ ] Experimental Mode: explicit acknowledgment required, unlocks advanced controls
- [ ] LUNA integration: LUNA explains each setting in plain language

**Done when:** A user can change their CPU governor from the Performance Lab UI and the change takes effect immediately.

---

### 5.2 — Accessibility

- [ ] Keyboard navigation: Tab/Shift-Tab cycles through all interactive widgets
- [ ] Focus ring: visible focus indicator on all interactive widgets (LUNA Green border)
- [ ] Enter/Space activates focused widget
- [ ] Reduced-motion mode: all animations snap to final state (animation engine intercept)
- [ ] High-contrast theme: built-in high-contrast variant of Luna Dark

**Done when:** The entire desktop is navigable without a mouse.

---

### 5.3 — Third-Party App Sandboxing

- [ ] AppArmor profiles generated for third-party apps at install time (DL-020)
- [ ] Third-party apps run with no capabilities by default
- [ ] Filesystem access: restricted to app-specific directories + XDG portals for user files
- [ ] Network access: must be declared in package manifest and confirmed by user at install

**Done when:** A third-party application cannot read files outside its declared scope.

---

### 5.4 — ABI/API Stability

- [ ] LGP protocol version field added to all messages (BLOCKER 5)
- [ ] LGP version negotiation: client sends `LGP_HELLO {version: 1}`, compositor confirms
- [ ] LunaGUI C API: all public symbols exported via `LUNAGUI_API` macro
- [ ] API stability guarantee: no symbol removed or signature changed within a minor version
- [ ] Changelog maintained: every API addition/removal documented
- [ ] `luna-abi-check` tool: validates a compiled binary's LunaGUI symbol usage against current ABI

**Done when:** An application compiled against LunaGUI v1.0 still loads correctly under LunaGUI v1.5.

---

### 5.5 — Remaining Volume V Documents

- [ ] Luna Performance Lab document (`Volume V / performance_lab.md`)
- [ ] Performance Profiles document (`Volume V / performance_profiles.md`)
- [ ] Component Ownership Matrix (`Volume II / 13_component_ownership.md`)
- [ ] LUNA Runtime document (`Volume IV / 00_luna_runtime.md`)
- [ ] Boot Recovery Modes (`Volume II / 14_boot_recovery.md`)
- [ ] Panic & Crash Strategy (`Volume II / 15_panic_strategy.md`)
- [ ] Versioning Policy (`Volume VII / versioning_policy.md`)
- [ ] Testing Philosophy (`Volume VII / testing_philosophy.md`)
- [ ] Security Review Checklist (`Volume VII / security_checklist.md`)
- [ ] Release Checklist (`Volume VII / release_checklist.md`)

---

## Progress Summary

```
STAGE 0 — Bare Metal          [ ] Not started
STAGE 1 — System Foundation   [ ] Not started
STAGE 2 — Graphics Primitive  [ ] Not started
STAGE 3 — Living Desktop      [ ] Not started
STAGE 4 — Full Presence        [ ] Not started
STAGE 5 — Polish               [ ] Not started
```

Update this summary every time a stage is completed.

---

## Decision Log Items Required Before Stage 2 Begins

These decisions were **blocking**, but have now been resolved:

| Decision needed | Blocks | Status |
|---|---|---|
| LGP wire format (TLV binary confirmed?) | 3.1 — Full LGP protocol | ✅ Resolved (DL-025) |
| GPU backend (Vulkan primary + EGL fallback) | 2.2 — lgp-render | ✅ Resolved (DL-026) |
| Software renderer for Stage 2 (yes/no) | 2.2 — lgp-render initial backend | ✅ Resolved (DL-026) |
| Root filesystem type (ext4 or Btrfs) | 0.4 — Root filesystem | ✅ Resolved (DL-027) |
| Default typeface selection | 3.3 — LunaGUI text rendering | ✅ Resolved (DL-028) |
| Text rendering library (FreeType + HarfBuzz) | 3.3 — LunaGUI text rendering | ✅ Resolved (DL-029) |

---

*Document: `Volume VII / implementation_roadmap.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-living*
*Status: Updated with every completed milestone*
*This document is the daily engineering reference. If you are writing code not listed here, you are either ahead of schedule or off course.*
