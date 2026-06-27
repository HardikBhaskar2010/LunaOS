# Mahina — Installer
**Volume V · Chapter 6**
**Classification:** Core Architecture — Userland
**Status:** Canonical · Specifies the Mahina installation process from ISO boot to first login

---

## Purpose

This document specifies the **Mahina installer** — the process that takes a user from a bootable Mahina ISO to a fully installed, configured system ready for first boot.

The installer is the first impression of Mahina for new users. It must be:
- **Fast** — nobody wants to wait 30 minutes to install an OS
- **Safe** — it must not destroy data without explicit confirmation
- **Beautiful** — consistent with Mahina visual language
- **Smart** — LUNA is present during installation (in a limited capacity)

---

## Overview

```
Installation flow (high level):

  ISO Boot
    │
    ▼
  Bootloader (limine) → installer kernel → luna-installer process
    │
    ├─ Welcome Screen
    ├─ Language & Locale
    ├─ Keyboard Layout
    ├─ Disk Selection & Partitioning
    ├─ System Configuration (hostname, timezone)
    ├─ User Account Creation
    ├─ Installation Summary (review before writing)
    ├─ INSTALL (write to disk)
    ├─ Post-Install (configure bootloader, initramfs)
    └─ First Boot Configuration
         ├─ AI Model Download (phi3:mini)
         └─ LUNA Online
```

---

## Architecture

### luna-installer Process

The installer runs as a full LunaGUI application with a restricted desktop environment:
- lgp-compositor running (full graphics)
- luna-installer as the only application window (FULLSCREEN)
- No luna-shell, no luna-dock, no luna-bar
- luna-ai-d in a limited mode (no LLM, presence-only)

```
Installer system services (running during install):
  lgp-compositor    — graphics
  luna-installer    — installation UI + logic
  luna-ai-d         — presence only (no LLM, guides the user)
  dhclient          — network (for AI model download)
  sshd (optional)   — remote installation support
```

### LUNA's Role During Installation

LUNA is present during installation as a **guide**, not as a fully-capable AI. Specifically:
- luna-ai-d runs in **INSTALL** mode (no LLM)
- LUNA Island is visible in AMBIENT state
- LUNA provides contextual text hints via templates (no LLM inference)
- Example: On the disk selection screen, LUNA says: "I'll only write to the disk you select. Your other drives are safe."

---

## Installer Screens

### Screen 1: Welcome

```
Welcome screen layout:

  ┌──────────────────────────────────────────────────────────────┐
  │                                                               │
  │                      ●  LUNA online.                         │
  │                                                               │
  │               Welcome to Mahina                              │
  │          A living operating system.                          │
  │                                                               │
  │                    [ Install Mahina ]                        │
  │                    [ Try without installing ]                │
  │                    [ Advanced options ]                      │
  │                                                               │
  │                Version 1.0 · x86_64                         │
  └──────────────────────────────────────────────────────────────┘
```

"Try without installing" boots to a live desktop session using tmpfs.

### Screen 2: Language & Locale

```
  Language:   [ English (US)           ▼ ]
  Region:     [ India                  ▼ ]   ← auto-detected from IP/locale
  Timezone:   [ Asia/Kolkata (IST)     ▼ ]   ← auto-detected
  Date format: ● DD/MM/YYYY  ○ MM/DD/YYYY
  Time format: ● 24-hour     ○ 12-hour
```

### Screen 3: Keyboard Layout

```
  Layout: [ English (US)         ▼ ]
  Variant: [ Default              ▼ ]

  Test field: [                                    ]
              ↑ Type here to test your keyboard layout
```

### Screen 4: Disk Selection

This is the most critical screen. It must be clear and safe.

```
Disk Selection:

  ╔══════════════════════════════════════════════════════════════╗
  ║  ●  LUNA: I'll only write to the disk you select.           ║
  ║           Your other drives remain untouched.               ║
  ╚══════════════════════════════════════════════════════════════╝

  Available disks:

  ┌──────────────────────────────────────────────────────────┐
  │  ○  sda  — 512 GB Samsung SSD  [Windows Boot Manager]   │  ← Contains OS
  │  ●  sdb  — 256 GB Kingston SSD  [Empty]                  │  ← Recommended
  │  ○  sdc  — 2 TB Seagate HDD    [Data: 1.8 TB used]      │
  └──────────────────────────────────────────────────────────┘

  Partitioning mode:
  ● Automatic (recommended) — Mahina manages partitioning
    Creates: EFI (512MB) + Root Btrfs (remaining space)
  ○ Manual — Advanced users only
  ○ Install alongside existing OS — Dual boot (v1.5)

  [ ⚠ Erase sdb and install Mahina ]   [ Back ]   [ Next ]
```

**Safety rules:**
- Any disk containing a detected OS (GRUB, Windows Boot Manager) shows a warning indicator
- "Erase" button text always includes the disk identifier — never just "Install"
- Manual partitioning requires acknowledging a warning before proceeding

### Screen 5: Partition Layout (Automatic)

```
Partition plan (review before writing):

  Disk: sdb (256 GB Kingston SSD)

  ┌─────────────────────────────────────────────────────────┐
  │  Partition 1  EFI System Partition   512 MB  FAT32      │
  │  Partition 2  Mahina Root            255 GB  Btrfs      │
  └─────────────────────────────────────────────────────────┘

  Btrfs subvolumes:
    @        → /           (root)
    @home    → /home
    @snapshots → /.snapshots

  ●  LUNA: Btrfs snapshots are enabled. You can roll back
           your system if something goes wrong after an update.
```

### Screen 6: System Configuration

```
  Hostname:   [ luna-machine            ]  (auto-suggested, editable)
  Timezone:   [ Asia/Kolkata (IST)      ]  (confirmed from Screen 2)
```

### Screen 7: User Account

```
  Full Name:  [                         ]
  Username:   [                         ]  (auto-derived from full name)
  Password:   [                         ]  (strength indicator)
  Confirm:    [                         ]

  ● Set this account as administrator
  ○ Standard account (no system modifications)

  ●  LUNA: I'll use your username to personalise your
           experience. You can change this any time.
```

### Screen 8: Installation Summary

```
  Review your choices:

  ┌───────────────────────────────────────────────────────────┐
  │  Language:    English (US)                                 │
  │  Timezone:    Asia/Kolkata (IST)                          │
  │  Keyboard:    English (US)                                │
  │  Disk:        sdb (256 GB) — ERASED AND REFORMATTED       │
  │  Partitions:  EFI 512 MB + Btrfs root 255 GB             │
  │  User:        Hardik Bhaskar (@hardik)  [administrator]   │
  │  Hostname:    luna-machine                                │
  └───────────────────────────────────────────────────────────┘

  ⚠  sdb will be completely erased. This cannot be undone.

  [ ← Back to change ]              [ Install Now ]
```

"Install Now" is the **final irreversible action**. After this button is pressed, disk writes begin.

### Screen 9: Installation Progress

```
  Installing Mahina...

  ┌──────────────────────────────────────────────────────────┐
  │  ████████████████░░░░░░░░░░░░░░░░  52%                   │
  └──────────────────────────────────────────────────────────┘

  ✅ Partitioning complete
  ✅ Btrfs filesystem created
  ✅ Base system installed (2.1 GB)
  ✅ Bootloader configured (limine)
  ⟳  Installing core applications...
  ○  Configuring system services
  ○  First-boot setup

  ●  LUNA: Base system installed. Setting up your environment.

  Estimated time remaining: 2 minutes
```

### Screen 10: First Boot Setup (Post-Install)

Runs on first actual boot into the installed system:

```
  ●  LUNA online.

  One more thing before we begin.

  LUNA works best with a language model installed locally.
  This is what lets me understand context and have real conversations.

  Model: Phi-3 Mini (3.8B parameters)
  Size:  2.1 GB
  Type:  Runs 100% locally. Never leaves your device.

  Download now?    [ Download (2.1 GB) ]   [ Skip for now ]

  If you skip, LUNA will still work — just without conversation ability.
  You can install the model later with: luna model install phi3:mini
```

---

## Installation Technical Details

### What Gets Installed

```
Installation payload:

  Base system:   ~800 MB
    Linux kernel 6.6.x LTS
    GNU coreutils
    glibc
    bash
    base library stack

  Mahina system: ~600 MB
    luna-init
    lgp-compositor
    luna-ai-d
    LunaGUI runtime
    Core applications (luna-shell, luna-terminal, luna-files,
                       luna-settings, luna-text, luna-notif,
                       luna-lock, luna-bar, luna-dock)

  AI model:      ~2.1 GB (optional — downloaded on first boot)
    phi3:mini via Ollama

  Total base:    ~1.4 GB (without AI model)
  Total with AI: ~3.5 GB
```

### Btrfs Subvolume Layout

```
Btrfs subvolume structure on installed system:

  / (Btrfs root)
  ├── @ → mounted at /           (root filesystem)
  ├── @home → mounted at /home   (user data)
  └── @snapshots → /.snapshots   (lpkg + manual snapshots)

  Rationale:
    Separate @home subvolume allows:
    - Rolling back root without affecting user data
    - Independent home backup/restore
```

### Bootloader Configuration

```
limine configuration written to EFI:

  # /boot/efi/limine.conf

  DEFAULT_ENTRY=mahina

  :Mahina
      PROTOCOL=linux
      KERNEL_PATH=boot:///boot/vmlinuz-linux
      MODULE_PATH=boot:///boot/initramfs-linux.img
      KERNEL_CMDLINE=root=UUID=<btrfs-uuid> rootflags=subvol=@ rw quiet
```

---

## Installer Error Handling

```
Error scenarios and responses:

  Insufficient disk space (< 5 GB free):
    → Block disk selection with error message
    → "Mahina requires at least 5 GB of free space."

  Disk write failure during installation:
    → Installation halts immediately
    → Error message with specific failure point
    → Disk is left in a partially-written state (user must repartition)
    → No data recovery promise for the target disk (it's being installed to)

  Network unavailable during AI model download:
    → Skip model download gracefully
    → Note in completion screen: "Network unavailable. Install model later."
    → Do NOT block installation completion on network availability

  Bootloader installation failure:
    → Common cause: Secure Boot incompatibility
    → Show specific error + link to troubleshooting docs
    → Do not silently fail — the system won't boot without a bootloader
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Bootloader: limine | DL-005 | ✅ Accepted |
| Filesystem: Btrfs with @, @home, @snapshots subvolumes | DL-027 | ✅ Accepted |
| AI model: optional at install, offered on first boot | DL-041, DL-042 | ✅ Accepted |
| LUNA present during installation (template mode, no LLM) | This document | ✅ Accepted |
| Dual boot support | v1.5 — not in v1 | 🔵 Draft |
| Manual partitioning UI | This document (basic), full spec pending | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Dual boot.** Dual boot with Windows / other Linux distros is a v1.5 feature. The installer must detect existing operating systems and handle EFI multi-boot correctly. Must be a fully separate design document.

2. **Manual partitioning UI.** The manual partitioning interface (creating, resizing, formatting partitions) is complex. The v1 installer shows manual partitioning as available but its full UI is not specified here. Must be designed before v1 ships.

3. **Encryption.** Full-disk encryption (LUKS) is not in v1. luna-ai-d memory is encrypted per DL-023, but the root filesystem is not. Whole-disk encryption is a high-demand feature. Must be a Decision Log entry for v1.5.

4. **OEM mode.** Should the installer support an OEM mode (pre-install for hardware vendors)? Vendors would want to image many machines. Must be a Decision Log entry.

5. **Offline AI model.** The default workflow downloads the AI model on first boot (requires internet). Should the ISO ship with the AI model embedded? Adds 2.1 GB to ISO size but enables fully offline installation. Must be a Decision Log entry.

---

## AI Context

- The installer is the **first impression**. If it looks wrong, the user's confidence in Mahina is damaged before the OS is even running. The installer must use full LunaGUI and Luna Dark visual language — not a minimal installer theme.
- LUNA's role in the installer is **guidance, not decision-making**. LUNA suggests, the user decides. Never make LUNA auto-select a disk or confirm a dangerous action.
- The "Erase" disk action is **irreversible**. The confirmation flow must make this crystal clear. The final button must say "Erase [disk identifier] and Install" — never just "Install" or "OK".
- The AI model download is **optional** at install time and must always have a "Skip" path. Network may not be available. The system must be fully functional without the AI model (just without LLM conversations).
- Post-install first boot is a separate phase. It runs in the fully installed system, not in the installer environment. This is where Ollama is configured and the model is pulled.

---

*Document: `Volume V / 06_installer.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume II/09_filesystem.md, Volume V/01_shell.md, Volume V/03_package_manager.md, DL-005, DL-027*
*Informs: Volume V/07_updater.md, Volume VI/06_milestones.md*
