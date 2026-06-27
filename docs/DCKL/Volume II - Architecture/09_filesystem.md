# Mahina — Filesystem Architecture
**Volume II · Chapter 9**
**Classification:** Core Architecture — Filesystem Layout
**Status:** Active · Reference for installer and package manager implementation

---

## Purpose

This document specifies the Mahina filesystem architecture: the directory hierarchy, the purpose and ownership of each directory, filesystem types used, mount strategy, and the rules governing where Mahina components may place files.

This document is the authoritative reference for:
- The installer's partition and directory setup
- `lpkg`'s file installation paths
- Any Mahina service that reads or writes to the filesystem
- AI coding agents determining where to place new files

---

## Overview

Mahina follows a filesystem hierarchy that extends and specializes the Linux FHS (Filesystem Hierarchy Standard) with Mahina-specific conventions in `/etc/luna/`, `/var/lib/luna*`, `~/.luna/`, and `/usr/lib/luna*`.

The hierarchy is intentional. Every directory exists because something needs it. No directory is created speculatively.

---

## Design Philosophy

**One location per concern.** A given type of file has exactly one canonical location. Configuration is in `/etc/luna/`. User AI data is in `~/.luna/`. Package-installed files are in `/usr/`. This rule prevents the pattern where a file could be in three different places depending on how it was installed.

**Documentation before creation.** Per Core Law VI (Documentation Is Code) and Core Law I (Own Every Layer): a directory added by a Mahina component must be documented here before the component places files in it. If this document does not list a directory, that directory should not exist in a clean Mahina install.

**Separation of user data and system data.** System configuration (how the OS behaves) lives in `/etc/luna/`. User preferences (how the AI layer adapts to this user) live in `~/.luna/`. These are never co-mingled. An OS update may overwrite `/etc/luna/` defaults; it must never touch `~/.luna/`.

---

## Architecture

### Root Filesystem Layout

```
/
├── boot/
│   ├── efi/                         # EFI System Partition mounted here (FAT32, DL-012)
│   │   └── EFI/
│   │       └── BOOT/                # limine EFI binary
│   │           └── BOOTX64.EFI
│   ├── vmlinuz-mahina               # Compiled kernel image
│   ├── initramfs-mahina.img         # Initial RAM filesystem
│   └── limine.cfg                   # Bootloader configuration (on ESP)
│
├── dev/                         # Device nodes (managed by devtmpfs)
├── proc/                        # Kernel process information (procfs)
├── sys/                         # Kernel device/driver info (sysfs)
├── run/                         # Runtime state (tmpfs — cleared on boot)
│   ├── luna-init.sock           # luna-init control socket
│   ├── luna-boot-complete       # Boot completion sentinel file
│   ├── dbus/
│   │   └── system_bus_socket   # D-Bus system bus socket
│   └── user/
│       └── <uid>/               # Per-user runtime files (XDG_RUNTIME_DIR)
│           └── pipewire-0       # PipeWire socket
│
├── tmp/                         # Temporary files (tmpfs — cleared on boot)
│
├── etc/
│   ├── luna/                    # Mahina system configuration (TOML)
│   │   ├── hostname             # System hostname (plain text)
│   │   ├── fstab.toml           # Filesystem mount table
│   │   ├── modules.conf         # Kernel modules to load at boot (TOML)
│   │   ├── sysctl.toml          # Kernel parameter overrides
│   │   ├── nftables.conf        # Firewall rules
│   │   └── services/            # luna-init service files (TOML)
│   │       ├── dbus.toml
│   │       ├── networkmanager.toml
│   │       ├── pipewire.toml
│   │       ├── wireplumber.toml
│   │       ├── ollama.toml
│   │       ├── lgp-compositor.toml   [name TBD]
│   │       ├── luna-shell.toml
│   │       ├── luna-bar.toml
│   │       ├── luna-island.toml
│   │       ├── luna-notif.toml
│   │       └── luna-ai-d.toml
│   ├── apparmor.d/              # AppArmor profiles
│   │   ├── luna-ai-d
│   │   ├── ollama
│   │   └── lgp-compositor      [name TBD]
│   └── dbus-1/
│       └── system.d/            # D-Bus system bus policy files
│           └── luna.conf
│
├── usr/
│   ├── bin/                     # User executables
│   │   ├── luna                 # luna CLI frontend
│   │   ├── luna-init-ctl        # luna-init control CLI
│   │   └── lpkg                 # Package manager
│   ├── sbin/                    # System executables (root-intended)
│   │   └── luna-init            # PID 1 (also placed in /sbin/ for initramfs)
│   ├── lib/
│   │   └── luna/                # Mahina internal libraries and data
│   │       ├── lgp/             # LGP compositor resources [TODO — Volume III]
│   │       └── themes/          # Theme data [TODO — Volume III]
│   └── share/
│       └── luna/                # Mahina shared data
│           ├── man/             # Man pages for all luna commands
│           └── licenses/        # License texts for bundled components
│
├── var/
│   ├── lib/
│   │   ├── lpkg/                # lpkg package database
│   │   │   ├── installed.db     # SQLite: installed packages
│   │   │   └── cache/           # Downloaded package archives
│   │   └── ollama/              # Ollama model weights and state
│   └── log/
│       ├── luna-init/
│       │   ├── boot.log         # Boot-time log (appended each boot)
│       │   └── runtime.log      # Post-boot luna-init runtime log
│       └── luna-ai-d.log        # LUNA.AI daemon log
│
└── sbin/
    └── luna-init                # Symlink to /usr/sbin/luna-init (initramfs compat)
```

### Per-User Package Installation Paths (DL-017)

Per DL-017, packages install per-user by default. The user home directory gains:

```
~/
├── .local/
│   ├── bin/                     # Per-user executables (on $PATH when set)
│   ├── lib/                     # Per-user libraries
│   ├── share/                   # Per-user shared data
│   └── share/lpkg/
│       └── installed.db         # Per-user lpkg package database (SQLite)
└── .luna/
    └── ... (AI data — see 06_memory.md)
```

Installation target selection:
- `lpkg install <pkg>` — installs to `~/.local/` (per-user, no privilege required)
- `lpkg install --system <pkg>` — installs to `/usr/` (system-wide, requires LUNA permission dialog or terminal auth per DL-016)

### User Home Directory Layout

```
~/
└── .luna/
    ├── memory/
    │   ├── patterns/
    │   │   ├── workflow.db          # SQLite: workflow patterns
    │   │   └── app_sequences.db     # SQLite: app launch sequences
    │   ├── context/
    │   │   └── session_history.db   # SQLite: session context
    │   └── preferences/
    │       └── learned.toml         # Inferred preferences
    ├── config/
    │   ├── observe.toml             # Observation allow-list (deny-by-default)
    │   └── luna.toml                # User behavior overrides for LUNA
    └── logs/
        └── luna-ai-d.log            # User-readable AI daemon log
```

The `~/.luna/` directory is fully documented in `06_memory.md`. It is not duplicated here — this document provides the canonical path. `06_memory.md` provides the content semantics.

---

## Current Decisions

### Filesystem Types

| Mount point | Filesystem | Rationale |
|---|---|---|
| `/` | ext4 or Btrfs | Root filesystem (DL-011 — Btrfs preferred for snapshot support; implementation under evaluation) |
| `/boot/efi` | FAT32 | EFI System Partition — standard Linux UEFI layout (DL-012) |
| `/tmp` | tmpfs | Cleared on boot, fast, never hits disk |
| `/run` | tmpfs | Runtime state — cleared on boot |
| `/dev` | devtmpfs | Kernel-managed device nodes |
| `/proc` | procfs | Kernel process information |
| `/sys` | sysfs | Kernel device/driver interface |

```
TODO:
Decision not yet finalized.
Reason: EFI partition strategy (separate FAT32 /boot/efi vs. merged /boot) has not
been formally decided. limine supports both.
A separate EFI partition is cleaner for UEFI compliance.
A merged /boot is simpler for the installer.
Must be a Decision Log entry before installer work begins.
```

### Configuration File Policy

All Mahina system configuration files:
- Live in `/etc/luna/`
- Use TOML format (DL-008)
- Are never auto-generated and never overwritten by software updates without user consent
- Are human-readable and human-editable

All user LUNA configuration files:
- Live in `~/.luna/config/`
- Use TOML format (DL-008)
- Are never read by any process except `luna-ai-d`
- Are never overwritten by system updates

### Package Installation Paths

`lpkg` installs packages to two possible target sets based on scope (DL-017):

**Per-user (default):**

| File type | Installation path |
|---|---|
| Executables | `~/.local/bin/` |
| Libraries | `~/.local/lib/` |
| Shared data | `~/.local/share/<package>/` |
| Package database | `~/.local/share/lpkg/installed.db` |

**System-wide (`--system` flag, requires privilege escalation via DL-016):**

| File type | Installation path |
|---|---|
| Executables | `/usr/bin/` |
| System executables | `/usr/sbin/` |
| Libraries | `/usr/lib/` |
| Headers | `/usr/include/` |
| Shared data | `/usr/share/<package>/` |
| Man pages | `/usr/share/man/` |
| Config templates | `/etc/<package>/` (or `/etc/luna/` for Luna-specific) |
| Package database | `/var/lib/lpkg/installed.db` |
| Package cache | `/var/lib/lpkg/cache/` |

---

## Technical Details

### File Ownership Rules

| Path | Owner | Group | Mode |
|---|---|---|---|
| `/etc/luna/` | root | root | `755` |
| `/etc/luna/services/*.toml` | root | root | `644` |
| `/var/lib/lpkg/` | root | root | `755` |
| `/var/lib/lpkg/installed.db` | root | root | `644` |
| `/var/lib/ollama/` | luna | luna | `750` |
| `/var/log/luna-init/` | root | root | `755` |
| `/var/log/luna-init/boot.log` | root | root | `644` |
| `~/.luna/` | user | user | `700` |
| `~/.luna/config/observe.toml` | user | user | `600` |
| `~/.luna/memory/` | user | user | `700` |
| `/run/luna-init.sock` | root | root | `600` |

### tmpfs Configuration

`/tmp` and `/run` are mounted as tmpfs at boot (Stage 2 — see `02_boot_flow.md`):

```toml
# /etc/luna/fstab.toml
[[mount]]
device     = "tmpfs"
mountpoint = "/tmp"
fstype     = "tmpfs"
options    = ["size=2G", "noexec", "nosuid", "nodev"]

[[mount]]
device     = "tmpfs"
mountpoint = "/run"
fstype     = "tmpfs"
options    = ["size=256M", "noexec", "nosuid", "nodev", "mode=755"]
```

The `/tmp` size limit of 2 GB prevents runaway processes from filling tmpfs and triggering OOM. The `noexec` flag prevents executables from running out of `/tmp` — a common attack vector.

### lpkg Database Schema

The package database at `/var/lib/lpkg/installed.db`:

```sql
CREATE TABLE packages (
    name         TEXT PRIMARY KEY,
    version      TEXT NOT NULL,
    description  TEXT,
    installed_at INTEGER NOT NULL,   -- Unix timestamp
    installed_by TEXT NOT NULL,      -- "user" | "dependency" | "bootstrap"
    files        TEXT NOT NULL,      -- JSON array of installed file paths
    depends      TEXT,               -- JSON array of dependency package names
    checksum     TEXT NOT NULL       -- SHA256 of package archive
);
```

This schema is intentionally minimal for v1. `lpkg` queries this database to:
- Detect conflicting files before installation
- Identify which package owns a given file (`lpkg owns /usr/bin/luna`)
- Resolve dependency graphs for removal

### Boot Partition

The boot partition contains:
- `vmlinuz-mahina` — the compiled kernel image
- `initramfs-mahina.img` — the initial RAM filesystem containing luna-init and minimal tools
- `limine.cfg` — bootloader configuration

Nothing else belongs in `/boot/`. Application binaries, libraries, and config files are never placed in `/boot/`.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Btrfs root filesystem with snapshots | Decision pending | Would allow pre-update snapshots for rollback |
| `/usr` merge | v1 | Ensure `/bin` and `/sbin` are symlinks to `/usr/bin` and `/usr/sbin` |
| Read-only root filesystem | v2 | Root mounted read-only; writable overlay for runtime state |
| `~/.luna/` encryption at rest | v2 | See `06_memory.md` Future Improvements |
| EFI partition auto-mounting | v1 | Ensure EFI partition is mounted at `/boot/efi` for firmware variable access |
| `lpkg` orphan detection | v1 | Detect and flag packages no longer referenced by any other package |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Root filesystem type.** Btrfs is the preferred candidate for snapshot support (DL-011). Final decision pending implementation evaluation.

2. ~~**EFI partition strategy.**~~ **Resolved (DL-012).** Standard Linux UEFI layout: FAT32 ESP mounted at `/boot/efi`.

3. **`/usr` merge.** Whether `/bin` → `/usr/bin` and `/sbin` → `/usr/sbin` are symlinks or separate directories. Decide before installation layout is finalized.

4. **LGP compositor resource paths.** `/usr/lib/luna/lgp/` is a placeholder. Actual paths depend on Volume III / 01_lgp.md decisions.

5. **Swap placement.** Swapfile at `/swapfile` vs. dedicated swap partition. Must be a Decision Log entry. See `06_memory.md` Open Question 1.

---

## AI Context

An AI agent creating files in Mahina must:

1. Check this document first. If the file type is listed in the installation paths tables, use the documented path. Do not create files in undocumented locations.
2. **Default installation is per-user (`~/.local/`)**, not system-wide. System-wide requires `--system` flag and privilege escalation (DL-017).
3. Never write files to `~/.luna/` except from `luna-ai-d`. Never write files to `/etc/luna/` except from `luna-init` or `lpkg`.
4. Respect TOML as the configuration format for all Mahina config files. See DL-008.
5. Service files belong in `/etc/luna/services/` with `.toml` extension.
6. AppArmor profiles belong in `/etc/apparmor.d/`.
7. Executables installed system-wide by `lpkg --system` go to `/usr/bin/` (user-facing) or `/usr/sbin/` (root-intended). Not `/usr/local/`.
8. Executables installed per-user by `lpkg` go to `~/.local/bin/`.
9. If a new component needs a directory not listed in this document, add the directory to this document before creating it.
10. `/run/` and `/tmp/` are tmpfs — cleared on every reboot. Persistent state belongs in `/var/lib/` (system) or `~/.luna/` (user AI data).
11. The EFI System Partition is at `/boot/efi` (FAT32). Do not place non-EFI files there.

---

*Document: `Volume II / 09_filesystem.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.3-draft*
*Depends on: architecture_overview.md, init_system.md, memory.md, security.md, core_laws.md (Law I, VI), decision_log.md (DL-008, DL-011, DL-012, DL-017), non_negotiables.md*
*Supersedes: v0.1-draft (EFI layout undecided, per-user install not documented)*
