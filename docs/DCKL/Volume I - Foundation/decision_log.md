# Mahina — Decision Log
**Volume I · Chapter 6**
**Classification:** Foundation Document — Architecture Record
**Status:** Append-only. Decisions are never deleted, only superseded.

---

## Purpose

This document records every significant architectural, design, or strategic decision made during Mahina development. It exists so that:

1. Future contributors understand *why* things are the way they are
2. AI tools (Claude Code, etc.) have full context when making changes
3. When a decision needs to be revisited, the original reasoning is available
4. We don't debate the same question twice

Format per entry:
```
## [DL-XXX] Decision Title
Date: YYYY-MM-DD
Status: ACCEPTED | REJECTED | SUPERSEDED by DL-XXX | PENDING
Decided by: Hardik Bhaskar

### Question
What were we deciding?

### Options Considered
What alternatives did we evaluate?

### Decision
What did we choose?

### Reasoning
Why this choice over alternatives?

### Consequences
What does this lock in or rule out?
```

---

## [DL-001] No Upstream Distro Base

**Date:** Project start
**Status:** ACCEPTED — Constitutional (see Core Laws)

### Question
Should Mahina base on an existing distribution (Arch, Debian, Void) or build fully from scratch?

### Options Considered
- **Arch Linux base** — Maximum flexibility, AUR available, familiar
- **Debian base** — Maximum stability, huge package ecosystem
- **Void Linux** — Already uses runit, musl-capable, lightweight
- **Linux From Scratch approach** — Full ownership, no inherited decisions

### Decision
Linux From Scratch approach. No upstream base.

### Reasoning
The entire premise of Mahina is that every layer is owned and understood. Using Arch or Debian would mean inheriting thousands of decisions we didn't make and can't fully account for. Specifically:
- We want to write `luna-init` — using Arch would mean coexisting with or replacing systemd, not starting clean
- We want to control the kernel config precisely — distro kernels include hundreds of drivers and options we don't need
- The narrative of "built from scratch" is central to the project's identity

### Consequences
- Full responsibility for system stability and package availability
- `lpkg` must handle everything systemd, pacman, etc. would have handled
- Build system complexity is significantly higher
- Cannot use AUR, PPA, or similar community package sources directly

---

## [DL-002] Init System: luna-init in C

**Date:** Project start
**Status:** ACCEPTED

### Question
Which init system should Mahina use?

### Options Considered
- **systemd** — Industry standard, feature-complete, complex
- **OpenRC** — Simpler, shell-based, used by Gentoo/Alpine
- **runit** — Minimal, reliable, used by Void
- **s6** — Extremely minimal, mathematically correct supervision
- **luna-init (custom)** — Written in C, TOML service files

### Decision
`luna-init` — custom init written in C with TOML-based service files.

### Reasoning
- systemd violates DL-001 (not understood at every layer, massive complexity)
- OpenRC is tempting but still "someone else's" init
- runit/s6 are excellent but we'd still be configuring someone else's tool
- Writing `luna-init` means we understand PID 1 completely
- C is the right language for PID 1 — minimal runtime, direct syscalls, crash-proof profile
- TOML service files are human-readable and version-controllable

### Consequences
- luna-init must handle: zombie reaping, filesystem mounting, service deps, shutdown sequencing
- We own every bug in PID 1 — this is a significant responsibility
- Phase 0: shell script init → Phase 1: C implementation

---

## [DL-003] Package Manager: lpkg in Python (v1)

**Date:** Project start
**Status:** ACCEPTED — with planned evolution

### Question
What language for `lpkg`?

### Options Considered
- **Python** — Fast to write, we know it well, readable codebase
- **Rust** — Faster execution, impressive on GitHub, type-safe
- **C** — Minimal runtime, fits the "no deps" philosophy

### Decision
Python for v1. Rust rewrite planned for v2 if performance matters.

### Reasoning
- We know Python deeply (FastAPI/Veronica experience)
- `lpkg` is not performance-critical — it runs occasionally, not continuously
- A working Python implementation in 2 weeks beats a partially working Rust implementation in 8
- SQLite + Python for the package database is a natural fit

### Consequences
- Python 3.12+ is a system dependency
- lpkg is installed before Python is managed by lpkg (bootstrapping problem — documented separately)
- v2 Rust rewrite is on the roadmap; Python codebase should be clean enough to reference

---

## [DL-004] Compositor: Hyprland (v1) → wlroots custom (v2)

**Date:** Project start
**Status:** ACCEPTED

### Question
Which Wayland compositor strategy for Mahina?

### Options Considered
- **Write custom compositor from scratch using wlroots**
- **Use Hyprland with deep custom config + IPC hooks**
- **Use Sway (i3-compatible, more stable)**
- **Use KWin (KDE's compositor)**

### Decision
Ship Hyprland for v1. Build wlroots-based custom compositor for v2.

### Reasoning
- Writing a custom compositor from scratch is months of work that doesn't ship v1
- Hyprland has excellent IPC — LUNA.AI can hook into window events without compositor code
- Hyprland's animation system is already hardware-accelerated and configurable
- From a user perspective, a deeply configured Hyprland *looks* completely custom
- v2 gives us time to learn wlroots properly while users get a real desktop in v1

### Consequences
- v1 has a Hyprland dependency — must write LBUILD for it
- Hyprland config changes can break Mahina shell behavior
- IPC socket path is Hyprland-specific — abstraction layer needed for v2 migration
- Custom compositor in v2 is not optional — it's needed for Luna Island proper layer implementation

---

## [DL-005] Bootloader: limine (v1)

**Date:** Project start
**Status:** ACCEPTED — GRUB2 deferred

### Question
Which bootloader for Mahina?

### Options Considered
- **GRUB2** — Universal, extensive theming examples, complex config
- **limine** — Modern, simpler config, UEFI-first, cleaner aesthetic
- **systemd-boot** — Not applicable (we don't use systemd)

### Decision
limine for v1.

### Reasoning
- limine has a cleaner configuration format
- Better suited for the cyberpunk boot screen aesthetic
- Less legacy baggage — designed for modern UEFI systems
- Simpler to build into our ISO creation pipeline

### Consequences
- Must write limine config in our `build-iso.sh`
- Some older BIOS systems may have compatibility questions — document minimum hardware
- Theming resources are less available than GRUB2 — we write our own anyway

---

## [DL-006] AI Runtime: Ollama

**Date:** Project start
**Status:** ACCEPTED

### Question
How should LUNA.AI serve local model inference?

### Options Considered
- **Ollama** — Simple REST API, model management built-in, cross-platform
- **llama.cpp direct** — Lower overhead, more control, requires more code
- **Hugging Face Transformers** — Python-native, huge model support, heavy

### Decision
Ollama for v1. llama.cpp direct for v2 if overhead is measurable.

### Reasoning
- Ollama's REST API matches our FastAPI daemon architecture perfectly
- `POST /api/generate` is one function call
- Model management (pull, list, delete) is handled for us
- llama.cpp direct would require us to maintain model loading code

### Consequences
- Ollama binary is a system dependency
- LBUILD file needed for Ollama
- Port: 11434 (Ollama default, internal only)
- Memory usage: Ollama holds model in RAM — must be accounted for in minimum specs

---

## [DL-007] C Library: glibc → musl (planned)

**Date:** Project start
**Status:** ACCEPTED — phased migration

### Question
Which C library for Mahina userspace?

### Options Considered
- **glibc** — Maximum binary compatibility, heavy, complex
- **musl libc** — Minimal, clean, static-linking friendly, some compat issues

### Decision
glibc for v1. musl migration planned for v2.

### Reasoning
- Fighting libc compatibility and building the OS at the same time is two battles
- glibc gives us access to more pre-compiled software during development
- musl's benefits (small footprint, static linking) become relevant once the stack is stable

### Consequences
- v1 images will be larger than they need to be
- musl migration is a breaking change — requires rebuilding all packages
- Plan musl as a v2.0 (New Moon) milestone

---

## [DL-008] Config Format: TOML

**Date:** Project start
**Status:** ACCEPTED

### Question
What config format for luna-init service files, lpkg manifests, luna.toml?

### Options Considered
- **TOML** — Readable, typed, python-toml library solid
- **YAML** — Familiar, but edge cases are notorious
- **INI** — Simple but no types
- **JSON** — Not human-writable

### Decision
TOML for all Mahina config files.

### Reasoning
- YAML's whitespace sensitivity causes hard-to-debug errors
- TOML is typed, readable, and has no surprising edge cases
- `tomllib` is in Python 3.11+ stdlib — no external dep for parser

### Consequences
- All service files use `.toml` extension
- All user config in `~/.luna/` is TOML
- Documentation must provide TOML examples, not YAML

---

## [DL-009] Kernel Version: Linux 6.6.x LTS

**Date:** Project start
**Status:** ACCEPTED — version pinned, upgrade on new LTS

### Question
Which Linux kernel version?

### Decision
6.6.x LTS. Track security patches only, not feature releases.

### Reasoning
- LTS kernels receive 6 years of support
- 6.6 supports: cgroups v2, BPF, io_uring, Wayland DRM, all hardware we care about
- Chasing latest kernel introduces instability during OS development phase

---

## [DL-010] LUNA.AI Port: 7734

**Date:** Project start
**Status:** ACCEPTED

### Question
What port does luna-ai-d listen on?

### Decision
`localhost:7734`. Internal only. Not exposed to network.

### Reasoning
- 7734 is not assigned by IANA to any standard service
- Memorable: 7734 upside down in a calculator spells "hELL" — cyberpunk.
- Firewall rules block this port from any external interface by default

---

## Pending Decisions

### [DL-P01] LUNA's name for users
Should LUNA address the user by: system username | user-set name | never | system-detected name
**Target:** Before v1 beta

### [DL-P02] Sound design
Does Mahina have UI sounds? Boot chime? Notification audio?
**Target:** Before v1 release

### [DL-P03] Default wallpaper
Original commission vs. generative art vs. procedural generator?
**Target:** Before v1 release

### [DL-P04] License
MIT vs. GPL v3
**Current leaning:** MIT — maximum adoption over control
**Target:** Before first public commit

### [DL-P05] Public release timing
Phase 2 (booting to desktop) for early community vs. Phase 4 (polished) for big launch?
**Target:** Phase 2 decision point

---

## Architecture Review Meeting #2 — Decisions

*Source: `Discussion_Session_2.md`*
*Integrated: 2026-06-26*

> **Documentation Conflict Note:**
> `Discussion_Session_2.md` used DL-005 through DL-018 as its internal numbering. These numbers collide with existing DL-005 through DL-010 in this log (limine, Ollama, glibc, TOML, kernel, port). The discussion document's decisions have been **renumbered** to DL-011 onwards in this canonical log. DL-004R is preserved as the supersession marker. The original DL-005–DL-010 entries above retain their original meanings. All Volume II documents are updated to reference the renumbered DL identifiers.

---

## [DL-004R] Graphics Architecture — Hybrid Model

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED — Supersedes DL-004
**Source:** Discussion_Session_2.md

### Question
What graphics architecture model serves both simple application developers and high-performance software?

### Decision
Mahina adopts a **hybrid graphics architecture**:
- Standard applications communicate through the **LunaGUI toolkit**
- Advanced applications may communicate directly with the **Luna Graphics Protocol (LGP)**

### Reasoning
- LunaGUI provides a simple, high-level API for application developers who do not need direct graphics control
- Direct LGP access preserves the ability for performance-critical applications (games, video editors, custom renderers) to bypass the toolkit layer
- Both paths are supported simultaneously — no capability is sacrificed

### Consequences
- LunaGUI toolkit is a required v1 deliverable — it is the primary application interface
- LGP remains the underlying protocol that LunaGUI uses internally
- Volume III must document both LGP (protocol) and LunaGUI (toolkit) as distinct but related components
- DL-004 (Hyprland compositor) is fully superseded — replaced by LGP compositor + LunaGUI

---

## [DL-011] Root Filesystem — Snapshot-Capable Strategy

**Date:** Architecture Review Meeting #2
**Status:** ❌ SUPERSEDED by DL-027
**Source:** Discussion_Session_2.md (was numbered DL-005 internally)

### Question
What filesystem strategy for the Mahina root partition?

### Decision
The root filesystem must prioritize **maximum performance** and **simple recovery**.

Automatic snapshots will be created before:
- System updates
- Kernel updates

Manual snapshots remain available at any time.

### Reasoning
- Pre-update snapshots provide a rollback path without requiring the user to understand backup tools
- Automatic snapshot creation before every destructive operation aligns with Core Law V (User Owns the Machine)

### Consequences
- Filesystem choice must support snapshots — Btrfs is the leading candidate; Ext4 requires a separate snapshot mechanism
- The installer must create the filesystem with snapshot support enabled
- `lpkg` must trigger pre-update snapshot creation before executing updates
- Final filesystem choice (Ext4 vs. Btrfs) requires a follow-up DL entry when implementation begins

---

## [DL-012] EFI Partition Layout

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-006 internally)

### Decision
Mahina follows the **standard Linux UEFI partition layout** for the EFI System Partition (ESP).

### Reasoning
- Preserves compatibility with existing firmware, dual-boot environments, and recovery tooling
- Reduces installer complexity
- Future internal directory structures within Mahina may differ, but the ESP remains standards-compliant

### Consequences
- ESP is FAT32, mounted at `/boot/efi` (standard location)
- limine config lives at the ESP-standard path
- The Open Question in `09_filesystem.md` regarding EFI layout is resolved: separate FAT32 ESP at `/boot/efi`

---

## [DL-013] Wireless Backend

**Date:** Architecture Review Meeting #2
**Status:** ❌ SUPERSEDED by DL-036
**Source:** Discussion_Session_2.md (was numbered DL-007 internally)

### Decision
Priority criteria: **maximum hardware compatibility** and **strong performance**.

### Reasoning
- Broad device support is required for v1 usability on real hardware
- Low latency matters for desktop responsiveness
- wpa_supplicant (broad compat) vs. iwd (modern, lower maintenance) remains under evaluation

### Consequences
- Final backend selection requires a follow-up DL entry after hardware compatibility testing
- NetworkManager will be used regardless of which backend is selected
- The Open Question in `10_networking.md` regarding wireless backend is partially resolved: criteria defined, implementation pending

---

## [DL-014] DNS Strategy

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-008 internally)

### Decision
Version 1.x uses the **existing Linux DNS resolver** (NetworkManager writes `/etc/resolv.conf` with DHCP-provided servers).

A future **LunaDNS** service may replace it after sufficient architectural research.

### Reasoning
- The existing resolver provides stability
- DNS is not a differentiating subsystem for v1
- Future LunaDNS can add DNS-over-TLS, local caching, and privacy features without blocking v1 delivery

### Consequences
- The Open Question in `10_networking.md` regarding DNS is resolved: use NetworkManager passthrough for v1
- No additional DNS daemon is required in the v1 service file set
- LunaDNS is a post-v1 architectural research item

---

## [DL-015] Time Synchronization

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-009 internally)

### Decision
Mahina uses the **existing Linux time synchronization service** (chrony or ntpd — standard upstream tool).

Time synchronization is not a differentiating subsystem for v1.

### Reasoning
- Reliability over reinvention for a non-differentiating subsystem
- chrony or ntpd are well-understood (Law I permits upstream tools we fully understand)

### Consequences
- The Open Question in `10_networking.md` regarding NTP is resolved: use an established upstream NTP tool
- NTP service is added to the luna-init service file set as a standard Stage 4 service
- A service file `/etc/luna/services/ntpd.toml` (or chrony equivalent) is required

---

## [DL-016] Package Privilege Escalation

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-010 internally)

### Decision
Package installation requiring elevated privileges requests authorization through the **LUNA graphical permission interface**.

Terminal authentication remains available as an alternative.

Graphical authorization is the preferred user experience.

### Reasoning
- The LUNA graphical interface is more accessible and consistent with the Living Interface design
- Terminal fallback preserves headless/recovery use cases
- LUNA presenting the authorization request maintains the "digital presence" identity (DL-015 AI layer always-running)

### Consequences
- The graphical permission interface must be implemented before `lpkg` can perform privilege escalation in the desktop session
- The LUNA Presence Engine (see DL-021) must be running for graphical authorization to function
- Terminal authentication fallback is required for non-graphical sessions
- The Open Question in `08_security.md` regarding lpkg privilege escalation is partially resolved: graphical + terminal, with graphical preferred

---

## [DL-017] Package Installation Scope

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-011 internally)

### Decision
Packages install **per-user by default**.

System-wide installation is available when explicitly requested by an administrator.

### Reasoning
- Per-user installation does not require privilege escalation for the common case
- System-wide installation remains available for shared system components
- Reduces the attack surface of the package manager for typical use

### Consequences
- `lpkg` must implement both per-user (`~/.local/`) and system-wide (`/usr/`) installation targets
- The default installation prefix is `~/.local/` unless `--system` is specified
- Per-user `lpkg` database lives at `~/.local/share/lpkg/installed.db`
- System `lpkg` database remains at `/var/lib/lpkg/installed.db`
- `09_filesystem.md` must be updated to document the per-user installation paths

---

## [DL-018] Package Transaction Rollback

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-012 internally)

### Decision
Every package transaction is **atomic** where possible.

On installation failure, `lpkg` automatically restores the previous system state.

Reliability takes precedence over partial installation.

### Reasoning
- A failed install that leaves the system in a broken state is worse than a failed install that cleanly rolls back
- Atomic transactions give the user confidence to install and try packages
- Aligns with Core Law V (User Owns the Machine — no irreversible actions without confirmation)

### Consequences
- `lpkg` must implement a transaction log: record every file operation before executing it
- On failure, replay the transaction log in reverse to restore previous state
- Rollback must handle: file removal, file restoration, database state
- This is a significant implementation complexity — must be scoped before lpkg v1 work begins
- Snapshot support (DL-011) provides an additional fallback for catastrophic failures

---

## [DL-019] Repository Policy

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-013 internally)

### Decision
Mahina supports:
- Official repositories
- Community repositories
- Third-party repositories

Security is achieved through **verification, not limitation**:
- Signature verification
- Reputation indicators
- Malware scanning
- Security analysis
- User warnings

### Reasoning
- Blocking software sources artificially restricts what Mahina can run
- Verification-based security provides protection without reducing capability
- Community and third-party repos are essential for a living ecosystem

### Consequences
- `lpkg` must implement signature verification for all repository types
- Reputation and malware scanning infrastructure must be designed (out of scope for v1 core, may be a v1.5 feature)
- Repository trust levels (official / community / third-party) must be communicated clearly in the UI
- Official repos are fully trusted. Community repos are signature-verified. Third-party repos show explicit user warnings.

---

## [DL-020] Third-Party Application Isolation

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-014 internally)

### Decision
Third-party applications execute inside an **isolated sandbox by default**.

Users may explicitly relax restrictions.

Security defaults favor containment without preventing advanced workflows.

### Reasoning
- Default sandboxing reduces the impact of malicious or poorly-written third-party software
- User override capability preserves power-user workflows
- Aligns with DL-019: accept all software sources but sandbox the untrusted ones

### Consequences
- A sandboxing mechanism is required (namespace isolation, seccomp, AppArmor profiles)
- "Third-party" must be defined precisely: is it by repository source, by signature trust level, or both?
- The sandbox must not prevent normal application functionality — it constrains what the app can access, not how it runs
- This is a significant security architecture deliverable — must be designed in `08_security.md` and Volume V

---

## [DL-021] AI Runtime Architecture — Two Independent Systems

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-015 internally)

### Decision
LUNA consists of two independent systems:

**LUNA Presence Engine** — starts automatically at boot:
- Luna Island
- Context awareness
- Expressions
- Notifications
- Lightweight behavior

**LLM Inference Engine** — loads lazily:
- Initializes only when: user starts conversation, voice interaction begins, AI automation requested, explicit reasoning required
- Minimizes idle memory consumption while preserving responsiveness

### Reasoning
- The Presence Engine provides always-on system presence with minimal resource cost
- The LLM (Ollama + model weights) is the heavyweight component — loading it at boot wastes ~2-3 GB of RAM during periods when the user is not conversing
- Lazy loading delivers the promise of "LUNA online" at boot without the full AI inference cost

### Consequences
- `luna-ai-d` is split into two logical components: presence daemon and inference engine
- The presence daemon starts at Stage 6 boot alongside shell components
- Ollama does not start at boot — it is launched by the inference engine on first LLM demand
- The memory layout in `06_memory.md` changes: Ollama model weights are NOT resident at boot
- `02_boot_flow.md` Stage 6 must be updated: Ollama is not a boot-time service
- Total boot-time RAM usage is significantly reduced — Presence Engine is lightweight (< 100 MB target)
- Luna Island, context tracking, and pattern observation are all Presence Engine functions
- LLM queries, conversation, voice, and automation are Inference Engine functions

---

## [DL-022] Context Service

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-016 internally)

### Decision
A **lightweight background context service** runs after boot.

During installation, the user explicitly grants or denies observation permissions.

Only approved data sources may be observed. No hidden monitoring.

### Reasoning
- Install-time explicit permission grant is cleaner than runtime popups asking for observation access
- User knows exactly what LUNA will observe before the system is running
- Aligns with Core Law II Privacy Sub-Law (deny-by-default observation) and Core Law IV (Silence Before Suggestion)

### Consequences
- The Mahina installer must include an observation permission configuration step
- `~/.luna/config/observe.toml` is populated during installation, not during first run
- The installer UI must clearly communicate what each observation permission does
- "Context service" = the Presence Engine's context awareness component (DL-021)

---

## [DL-023] Persistent Memory

**Date:** Architecture Review Meeting #2
**Status:** ACCEPTED
**Source:** Discussion_Session_2.md (was numbered DL-017 internally)

### Decision
LUNA **maintains memory across reboots**.

During shutdown, a protected summarization process produces a condensed memory record.

This memory is **encrypted** and stored in a dedicated protected location.

Long-term memory remains entirely under user control.

### Reasoning
- Persistent memory makes LUNA progressively more useful over time — she remembers your patterns across sessions
- Summarization at shutdown prevents the memory store from growing unboundedly
- Encryption protects user behavioral data at rest (addresses the Open Question in `06_memory.md`)
- User control of all memory data is non-negotiable (Core Law II, Core Law V)

### Consequences
- Memory encryption at rest is a v1 requirement, not a v2 deferral
- A shutdown hook in luna-init must trigger the summarization process before stopping services
- Summarization process must complete within the shutdown timeout (current: 5 seconds total — may need adjustment)
- Encryption key management must be designed — likely tied to user login credentials
- `06_memory.md` must be updated: memory encryption is ACCEPTED, not deferred
- `luna memory --clear` must also clear the encrypted persistent summary

---

## [DL-024] Mahina Success Criteria

**Date:** Architecture Review Meeting #2
**Status:** CANONICAL
**Source:** Discussion_Session_2.md (was numbered DL-018 internally)

### Decision
Version 1.0 succeeds when:
- The operating system **feels technically impressive**
- The operating system **genuinely feels alive**

Neither goal may come at the expense of the other.

**Performance and Presence are equal pillars of Mahina.**

### Consequences
- Every architectural and implementation decision is evaluated against both criteria simultaneously
- A feature that is technically impressive but makes the system feel lifeless does not ship
- A feature that creates presence but degrades performance does not ship
- DL-018 is canonical — it cannot be amended by a future DL entry, only by full project review

---

## [DL-025] LGP Wire Format — TLV Binary
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P01 (pending)

### Decision
The Luna Graphics Protocol uses **TLV (Type-Length-Value)** binary framing for all wire messages. Each message consists of a 1-byte type field, a 4-byte length field, and an N-byte payload. No external serialization framework is required.

### Rationale
LGP must be dependency-free, easy to debug in C, and simple to evolve. TLV provides append-compatible message evolution, excellent hex-dump debuggability, and a minimal parser footprint. Schema-driven alternatives (Cap'n Proto, FlatBuffers) add build-time dependencies that contradict Mahina's local-first, minimal-dependency philosophy.

### Consequences
- All compositor and client implementations encode/decode LGP messages using the TLV format
- The `lgp_message_t` header struct includes: `uint8_t type`, `uint32_t length`, followed by the payload
- Message versioning is handled via the `LGP_HELLO` handshake version field, not the wire framing
- Volume III / 01_lgp.md protocol specification is now finalized and implementation may begin

---

## [DL-026] GPU Backend Strategy — Staged Implementation
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P02 (pending), DL-P03 (pending)

### Decision
GPU backend is implemented in two stages:
- **Stage 2:** Software renderer (CPU blit to dumb framebuffer). Proves the compositor pipeline, LGP protocol, and rendering architecture before GPU complexity is introduced.
- **Stage 3:** Vulkan as the primary GPU renderer. OpenGL/EGL as a fallback for hardware without Vulkan 1.1+ support.

The compositor communicates exclusively with the `lgp-render` abstraction layer. GPU implementation details are isolated behind that layer.

### Rationale
Separating "compositor protocol works" (Stage 2) from "GPU backend works" (Stage 3) reduces risk. The software renderer allows full LGP protocol development and testing on any machine. Vulkan is chosen as the primary backend for its performance ceiling, explicit synchronization, and DMA-BUF zero-copy import. EGL fallback ensures support on hardware without Vulkan drivers.

### Consequences
- Stage 2 implementation uses a software renderer only. No GPU API calls in Stage 2.
- Stage 3 implementation introduces Vulkan backend behind `lgp-render` API
- No compositor code outside `lgp-render/` may call Vulkan or EGL directly
- Volume II / 02_rendering_pipeline.md GPU backend section is now resolved

---

## [DL-027] Root Filesystem — Btrfs
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-011 (was Experimental/Provisional)

### Decision
Mahina uses **Btrfs** as the root filesystem. ext4 is not the primary target.

Automatic Btrfs snapshots are created before:
- System updates
- Kernel updates
- Driver updates

Manual snapshots remain available at any time.

### Rationale
The snapshot requirement is a first-class Mahina feature, not an afterthought. Btrfs delivers native snapshots, copy-on-write, and rollback capability that align with Mahina's recovery architecture. ext4 cannot deliver these natively. DL-011 was Provisional pending this confirmation — it is now superseded.

### Consequences
- Installer partitions the root filesystem as Btrfs
- btrfs-tools included in initramfs
- Swapfile created with `chattr +C` (CoW disabled for swapfile performance — see DL-038)
- lpkg daemon creates a Btrfs snapshot before every update transaction (DL-018 atomicity extended to filesystem level)
- Volume II / 09_filesystem.md updated to reflect Btrfs as final decision

---

## [DL-028] Default System Typeface — Bitcount
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P05 (pending — was recommending Inter)

### Decision
**Bitcount** is the canonical Mahina system font for personality-forward contexts:
- Boot screen
- Login
- Dock
- Luna Island
- LUNA responses
- Branding
- Major UI headings

For dense reading contexts (documentation, terminals, editors, productivity applications), Mahina uses optimized companion fonts selected for readability. The companion font is to be determined in a future design decision.

### Architectural Principle (from AR-004)
> Personality where it matters. Readability where it matters.

### Rationale
Bitcount provides Mahina with a distinctive visual identity that no other operating system shares. Generic screen fonts (Inter, Noto) would produce a desktop that looks like any other Linux distribution. The split usage policy ensures personality never comes at the expense of readability for extended reading tasks.

### Consequences
- Bitcount font files ship with Mahina base install
- `LunaTheme.current().typography.family_display` = "Bitcount"
- A separate `typography.family_reading` token will hold the companion font (pending DL entry)
- Volume III / 06_theme_engine.md typeface section updated to Bitcount

---

## [DL-029] Text Rendering Library — FreeType + HarfBuzz
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P06 (pending)

### Decision
LunaGUI uses **FreeType** for glyph rasterization and **HarfBuzz** for text shaping. No custom text rendering engine will be developed for Version 1.

### Rationale
FreeType + HarfBuzz is the industry standard for Linux text rendering. It provides mature Unicode support, complex script shaping (Arabic, Devanagari, CJK), high-quality hinting, and excellent community maintenance. No alternative offers comparable correctness for an OS-level toolkit.

### Consequences
- `libfreetype` and `libharfbuzz` are first-party dependencies in LunaGUI
- Volume III / 04_lunagui.md text rendering section is now resolved

---

## [DL-030] LunaGUI Layout Engine — Flexbox Model v1
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P07 (pending)

### Decision
LunaGUI v1 uses a **flexbox-style box model** layout engine: horizontal and vertical containers with grow/shrink semantics. Constraint-based layout may be introduced in v1.5 or later.

### Rationale
The flexbox model handles the vast majority of Mahina application layouts (settings panels, file managers, status bars, dialogs) with minimal implementation complexity. A constraint solver is significantly harder to implement correctly and debug, and is not required for v1 UI targets.

### Consequences
- `LunaPanel` widget supports `direction: horizontal | vertical`, `gap`, `align`, `justify` properties
- No constraint solver is implemented in v1
- Volume III / 04_lunagui.md layout engine section is now resolved

---

## [DL-031] Compositor Readiness Signal — D-Bus
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P09 (pending)

### Decision
The LGP compositor signals readiness by publishing a **D-Bus signal**: `org.mahina.compositor.Ready`. Stage 6 services wait on this signal before connecting to the compositor socket.

### Rationale
D-Bus is already running at Stage 4. Using it for the compositor readiness signal is architecturally consistent, eliminates polling, and avoids sentinel file proliferation in `/run/`.

### Consequences
- lgp-compositor registers on D-Bus at startup and emits `Ready` after KMS mode set and LGP socket creation
- luna-init Stage 6 uses `dbus-wait` or equivalent to block until the signal is received
- All Stage 6 service startup scripts depend on this D-Bus signal

---

## [DL-032] Input Backend — libinput
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P10 (pending)

### Decision
The LGP compositor uses **libinput** for all input device management.

### Rationale
Mahina will not reimplement years of touchpad compatibility, pointer acceleration, gesture recognition, and device quirk handling. libinput provides all of this and allows engineering effort to focus on Mahina-specific components. Raw evdev is not a viable alternative for a complete OS.

### Consequences
- `libinput` is a first-party dependency of lgp-compositor
- The compositor opens a libinput context at startup, reads events in the main thread event loop
- No process other than lgp-compositor opens `/dev/input/event*` directly
- Volume III / 03_compositor.md input backend section is now resolved

---

## [DL-033] Clipboard Architecture — LGP Extension
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P08 (pending)

### Decision
The Mahina clipboard is implemented as **LGP protocol extension `lgp_ext_clipboard_v1`**. The compositor owns clipboard state. Clipboard access is governed by the Permission Engine. Applications never communicate clipboard data directly with one another.

### Rationale
Clipboard ownership by the compositor is architecturally consistent — the compositor already routes input and surfaces. Applications declaring clipboard ownership do so via LGP, and the compositor brokers read requests. This model prevents clipboard snooping by third-party applications without explicit permission.

### Consequences
- `lgp_ext_clipboard_v1` extension specification to be written in Volume III
- Applications request clipboard write via `LGP_CLIPBOARD_SET`
- Applications request clipboard read via `LGP_CLIPBOARD_GET` (Permission Engine may prompt)
- D-Bus clipboard service is not created — LGP extension is the canonical clipboard interface

---

## [DL-034] Luna Island Interaction — Hybrid Model
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P13 (pending)

### Decision
Luna Island uses a **hybrid interaction model**:
- **Short click:** Expand Luna Island into a compact interaction panel
- **Long press:** Expand into the complete conversational interface

### Rationale
This model preserves LUNA's ambient presence (short click is a lightweight check-in) while providing the full conversational interface on deliberate long-press interaction. The distinction communicates to users that LUNA has both a lightweight and a full-attention mode.

### Consequences
- luna-island implements two expansion states: `COMPACT_PANEL` and `FULL_CONVERSATION`
- Both states are owned by `luna-island` (see DL-044)
- Short click → `Expand` animation (≤ 300ms) to compact panel
- Long press (≥ 500ms) → `Expand` animation to full conversational interface
- Volume IV / 00_luna_runtime.md LUNA Island section updated

---

## [DL-035] Screen Lock Ownership — luna-lock
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P11 (pending)

### Decision
Screen lock is owned by a dedicated process: **`luna-lock`**. It is not owned by luna-init or luna-shell.

`luna-lock` characteristics:
- Privileged daemon supervised by luna-init
- Creates a `LAYER_SYSTEM_MODAL` LGP surface covering all other surfaces
- Handles PAM authentication
- Has an AppArmor profile restricting its capabilities

### Rationale
luna-init should not render graphics. luna-shell is lower-trust and could theoretically be killed to bypass a shell-owned lock. A dedicated `luna-lock` process provides better privilege separation, an independent lifecycle, and cleaner security boundaries.

### Consequences
- `luna-lock` added to Volume VII / implementation_roadmap.md as a Stage 3 deliverable
- luna-init starts `luna-lock` when the user requests screen lock (D-Bus method or hotkey)
- `LAYER_SYSTEM_MODAL` (600) is the lock surface layer — nothing above it except `LAYER_CURSOR` (700)
- Volume II / 13_component_ownership.md screen lock section updated

---

## [DL-036] Wireless Backend — wpa_supplicant v1
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-013 (was Draft — criteria defined, choice pending)

### Decision
Mahina v1 uses **wpa_supplicant** as the Wi-Fi authentication backend, managed by NetworkManager. Migration to iwd may be reconsidered after broader hardware validation in a future release.

### Rationale
Maximum hardware compatibility is the highest networking priority for v1. wpa_supplicant has broader device support than iwd on current Linux hardware. The NetworkManager integration for wpa_supplicant is mature and well-tested.

### Consequences
- `wpa_supplicant` included in the base OS image
- NetworkManager configured to use wpa_supplicant backend
- iwd is not installed by default in v1
- DL-013 (criteria) is now superseded — the choice is made

---

## [DL-037] Theme Change Notifications — Dual Channel
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004

### Decision
Theme change notifications are delivered through **both** channels:
- **LGP:** `LGP_THEME_CHANGED` broadcast to all connected graphical clients
- **D-Bus:** `org.mahina.theme.Changed` signal for non-graphical services

### Rationale
Graphical clients use the LGP channel they already have open. Non-graphical services (CLI tools, system daemons that format output with theme-aware colors) need D-Bus. The implementation cost of both channels is negligible.

### Consequences
- Compositor emits both signals simultaneously on theme switch
- LunaGUI handles `LGP_THEME_CHANGED` to re-read `LunaTheme.current()`
- Volume III / 06_theme_engine.md theme switch section updated

---

## [DL-038] Swap Strategy — Swapfile + zram Hierarchy
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004

### Decision
Mahina uses a **swapfile + zram** combination with the following memory hierarchy:
1. RAM (primary)
2. zram (compressed RAM — first swap tier)
3. Swapfile at `/swapfile` (second swap tier, disk-backed)

### Rationale
Swapfile is simpler than a dedicated swap partition (resizable, no repartitioning, works on Btrfs with `chattr +C`). zram as the first tier provides fast, RAM-based swap that reduces disk I/O for most workload spikes. The swapfile provides a safety net for extreme memory pressure.

### Consequences
- Installer creates `/swapfile` on the Btrfs root with CoW disabled (`chattr +C`)
- zram configured at boot by luna-init (from Volume II / 06_memory.md)
- `vm.swappiness = 10` maintained (swapping is the last resort)

---

## [DL-039] Built-in Themes v1 — Luna Dark Only
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P15 (pending — was recommending shipping both light and dark)

### Decision
Mahina Version 1 ships **Luna Dark only**. Light mode is intentionally postponed to a future release.

### Rationale
Dark mode is Mahina's visual identity. LUNA's expressions, Island behavior, motion language, semantic color semantics, and particle effects are designed for dark environments. Shipping a light mode in v1 that has not received the same design attention would undermine the coherence of the visual identity. Light mode will be added when it can be designed to the same standard.

### Consequences
- Theme picker in v1 shows Luna Dark as the only built-in option
- Community themes (including light variants) may be installed via lpkg in v1
- Volume III / 06_theme_engine.md light mode section updated to reflect this decision

---

## [DL-040] Accessibility — AT-SPI2
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P12 (pending)

### Decision
Mahina exposes accessibility information via **AT-SPI2** (Assistive Technology Service Provider Interface v2). Existing Linux screen readers (Orca, etc.) work without modification.

**v1 minimum:** Full keyboard navigation in all LunaGUI widgets.
**v1.5 target:** AT-SPI2 bridge allowing external screen readers to interrogate the widget tree.

### Rationale
Accessibility is a first-class requirement. Using the AT-SPI2 standard means existing assistive technology works without Mahina-specific modifications. A custom protocol would require screen reader developers to implement Mahina-specific support — an unacceptable barrier.

### Consequences
- LunaGUI v1 implements keyboard navigation for all interactive widgets (Tab, Shift-Tab, Enter, Space)
- LunaGUI v1.5 implements AT-SPI2 D-Bus interface
- A `luna-a11y` bridge process or in-process AT-SPI2 provider to be designed before v1.5

---

## [DL-041] Voice / TTS — Optional Local, Disabled by Default
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P14 (pending)

### Decision
Local TTS ships as an **optional component** in v1. Default state: **disabled**. Users explicitly enable voice functionality. No mandatory online voice services.

### Rationale
TTS architecture must be in scope for v1 so the AI runtime can support it, but forcing it on by default would increase the default install footprint and surprise users. The opt-in model respects user choice while ensuring voice capability is available.

### Consequences
- A local TTS engine (Piper TTS or equivalent) is packaged as an optional lpkg component
- LUNA personality Priority 7 (spoken dialogue) activates only when TTS is enabled by the user
- PipeWire (already running) handles audio output for TTS
- Settings application includes a Voice section with an enable toggle

---

## [DL-042] AI Runtime Memory — Dynamic, No Fixed Reservation
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** Earlier documentation recommendation of a static 6 GB cgroup cap

### Decision
The AI runtime memory model has two tiers:

**Presence Engine:** Always running. Intentionally lightweight. No fixed memory ceiling beyond the `luna-ai.slice` soft limit.

**LLM Runtime:** Loads lazily on first demand. Memory allocation scales according to:
- Selected model size
- Available system RAM
- User configuration

**No fixed memory reservation exists.** The `luna-ai.slice` cgroup enforces a limit appropriate to the hardware at install time, not a hardcoded value.

### Architectural Principle (from AR-004)
> The Presence Engine is always alive. The Intelligence Engine wakes only when needed. Presence is continuous. Reasoning is on demand.

### Rationale
A fixed 6 GB cap was a documentation estimate that does not account for model variation (a 3B model needs ~2 GB, a 13B model needs ~8 GB) or hardware variation (a 32 GB system should allow more than a 16 GB system). Dynamic scaling is correct architecture.

### Consequences
- luna-ai-d queries available system RAM at startup and sets `luna-ai.slice memory.max` accordingly
- The Presence Engine target: < 200 MB RAM at all times
- The LLM Runtime cap: configurable, defaulting to (total_ram / 4), minimum 2 GB, maximum 12 GB
- OOM score for Ollama: high (killed first under system memory pressure)
- Volume IV / 00_luna_runtime.md memory section updated

---

## [DL-043] Boot Splash Transition — Accept the Brief Cut
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004

### Decision
Mahina Version 1 accepts a **brief visual transition** (single black frame, ~16ms at 60Hz) between the luna-init framebuffer boot splash and the LGP compositor's first frame. No architectural complexity is introduced to eliminate this cut.

### Rationale
Eliminating the cut requires the compositor to start before Stage 4 system services are complete — a bootstrapping problem that creates more complexity than it solves. One frame of black at 60Hz is perceptually imperceptible. Visual polish of the boot transition can be improved in a future release without affecting system architecture.

### Consequences
- Boot splash renderer (luna-init) stops rendering before compositor takes over
- lgp-compositor's first frame is its own rendered output
- The brief cut is documented as intentional behavior, not a bug
- Volume II / 02_rendering_pipeline.md boot splash handoff section updated

---

## [DL-044] Conversation Panel Ownership — luna-island
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Supersedes:** DL-P20 (pending)

### Decision
The conversation panel (both compact and full modes, per DL-034) is owned by **`luna-island`**. No additional process is introduced.

### Rationale
The conversation panel is an expanded state of LUNA's presence, not a separate application. luna-island already owns the LUNA_ISLAND surface. Expanding that surface to contain conversation UI is architecturally simpler than spawning a second graphical process and compositing two surfaces together.

### Consequences
- luna-island implements three visual states: `AMBIENT` (compact indicator), `COMPACT_PANEL` (short click), `FULL_CONVERSATION` (long press)
- Transitions between states use `Expand` / `Collapse` motion classes (≤ 300ms)
- luna-ai-d conversation API remains in luna-ai-d — luna-island calls D-Bus to send/receive messages
- Volume IV / 00_luna_runtime.md and Volume II / 13_component_ownership.md updated

---

## [AP-001] Architectural Principle — Typography Philosophy
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Type:** Architectural Principle (non-decision record)

> Typography exists to communicate personality without sacrificing readability.
> Distinctive branding fonts are used where identity matters.
> Optimized reading fonts are used where productivity matters.
> Personality should never reduce usability.

This principle governs all future typography decisions in LunaGUI, the theme engine, and built-in applications.

---

## [AP-002] Architectural Principle — AI Runtime Philosophy
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-004
**Type:** Architectural Principle (non-decision record)

> The Presence Engine is always alive.
> The Intelligence Engine wakes only when needed.
> Presence is continuous.
> Reasoning is on demand.

This principle governs all future luna-ai-d architecture decisions, memory allocation, and startup behavior.

---

## [DL-045] Operating System Identity Renamed to Mahina
**Date:** 2026-06-27
**Status:** ✅ Accepted
**Session:** AR-006

### Question
How do we resolve the ecosystem naming conflict and set up the operating system for long-term scalability without abandoning its core philosophical identity?

### Options Considered
- Keep the name **LunaOS** and accept the ecosystem conflicts.
- Create a completely new identity disconnected from the moon/lunar theme.
- Rename the operating system to **Mahina** (meaning Moon), preserving LUNA as the digital presence within it.

### Decision
The operating system shall officially be known as **Mahina**. LUNA remains the digital presence (AI) within the operating system.

### Reasoning
Mahina provides a unique identity with high searchability and low ecosystem conflict, while preserving the project's original philosophical foundation. Separating the OS (Mahina) from the AI (LUNA) creates a cleaner layered identity: Mahina is the world, LUNA is the consciousness that inhabits it.

### Consequences
- All canonical public references to the OS are updated from LunaOS to Mahina.
- The Luna ecosystem (LunaGUI, Luna Island, LGP, lpkg, LUNA runtime) remains unchanged.
- Supersedes all previous documentation identifying the OS as LunaOS.

---

*Document: `00_Foundation/decision_log.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*This document is append-only. Add new entries at the top of the numbered section.*
