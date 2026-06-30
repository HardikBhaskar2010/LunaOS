# Mahina — Core Applications
**Volume V · Chapter 5**
**Classification:** Core Architecture — Userland
**Status:** Canonical · Defines the minimum set of applications shipped with Mahina v1

---

## Purpose

This document defines the **core application set** — every application that ships with Mahina as part of the base install. These are the applications users expect to find when they first boot into a Mahina desktop.

Core applications are:
- Maintained by the Mahina project (not third-party)
- Built using LunaGUI (consistent visual language)
- Integrated with LUNA's presence system
- Shipped in every Mahina ISO

---

## Core Application Inventory

```
v1 Core Applications:

  System (boot before desktop):
    luna-init        — init system (Volume II/04)
    lgp-compositor   — graphics compositor (Volume III/03)
    luna-ai-d        — AI presence daemon (Volume IV/00)
    luna-lock        — screen lock (DL-035)
    luna-screenshot  — screenshot and OCR utility

  Desktop Layer:
    luna-shell       — desktop shell, launcher, workspaces (Volume V/01)
    luna-island      — LUNA presence surface (Volume III/07)
    luna-bar         — status bar (part of luna-shell)
    luna-dock        — application dock (part of luna-shell)
    luna-notif       — notification daemon (this document)

  User Applications:
    luna-terminal    — terminal emulator (Volume V/02)
    luna-files       — file manager (this document)
    luna-settings    — system settings (this document)
    luna-text        — text editor (this document)

  System Services:
    luna-netd        — network management daemon (thin wrapper over NetworkManager)
    luna-power       — power management daemon (battery, suspend, shutdown)
    luna-audio       — audio management (PipeWire session manager, v1.5)
    luna-update      — update daemon (Volume V/07)
```

---

## luna-notif (Notification Daemon)

### Role

luna-notif is the notification daemon. It receives notification requests from all applications via the freedesktop-compatible D-Bus interface (`org.freedesktop.Notifications` / `org.mahina.notify`) and decides how to display them.

### Notification Pipeline

```
Application sends notification
       │
       ▼
luna-notif receives (D-Bus)
       │
  ┌────▼────────────────────────┐
  │ Priority classifier         │
  │ (urgent / normal / low)     │
  └────┬──────────────┬─────────┘
       │              │
       ▼              ▼
  Show as toast   Queue (if Focus mode)
  (luna-island    or Dismiss (if Gaming mode)
   NOTIFICATION
   _TOAST layer)
       │
       ▼
  Persist to notification center
  (available via luna-bar click — v1.5)
```

### Toast Display Rules

```
Toast display rules:

  GAMING mode:    All notifications queued (no toasts shown)
  FOCUS mode:     Only URGENT notifications shown as toast
  STUDY mode:     Only URGENT notifications shown
  DEVSHELL mode:  NORMAL and URGENT shown; LOW queued
  AMBIENT mode:   All notifications shown as toast

  Toast auto-dismiss:
    LOW priority:    5 seconds
    NORMAL priority: 8 seconds
    URGENT priority: No auto-dismiss (requires user action)

  Max simultaneous toasts: 5 (oldest dismissed to make room)
```

### Notification Hints (Mahina-specific)

```
Mahina-specific hints in the notification `hints` dict:

  "luna-category":   string — semantic category for LUNA context
                     Values: "build", "git", "system", "app", "message"

  "luna-action-cmd": string — shell command to suggest running
                     (appears as "Run: <cmd>" action in toast)

  "luna-priority":   string — override priority: "low" | "normal" | "urgent"
```

---

## luna-files (File Manager)

### Role

luna-files is the Mahina graphical file manager. It is the default handler for `inode/directory` MIME type.

### Layout

```
luna-files window layout:

  ┌────────────────────────────────────────────────────────────┐
  │  ← → ↑  [  /home/user/projects/lunagui  ]     🔍  [⊞][≡]│  ← Toolbar
  ├────────────┬───────────────────────────────────────────────┤
  │  Bookmarks │  Name            ↑   Size      Modified       │
  │  ─────     │  ────────────────────────────────────────────  │
  │  🏠 Home   │  📁 src/                 —       Jun 27       │  ← File grid
  │  📁 Project│  📁 docs/                —       Jun 26       │
  │  📁 Luna   │  📄 README.md           4 KB     Jun 25       │
  │  Downloads │  📄 luna.toml           1 KB     Jun 20       │
  │  Documents │                                                │
  │            │                                                │
  │  ─────     │                                                │
  │  Devices   │                                                │
  │  💿 sda1   │                                                │
  └────────────┴───────────────────────────────────────────────┘
  │  3 items, 2 folders, 1 file              32.1 GB free      │  ← Status bar
  └────────────────────────────────────────────────────────────┘
```

### Features (v1)

- Directory navigation (forward, back, up, breadcrumb)
- List view and grid view toggle
- File operations: copy, cut, paste, rename, delete (to trash)
- Bookmarks sidebar (Home, custom locations, connected devices)
- File search (name-based, v1; content-based v1.5)
- MIME-type based default application launch (double-click to open)
- Btrfs snapshot browser (right-click directory → "Browse snapshots")

### LUNA Integration

```
luna-files publishes to org.mahina.context.Files:

  DirectoryOpened(path: string)
  → Context Engine: user is browsing files at this path
  → If path matches a known project root: updates project context

  FileOpened(path: string, mime_type: string)
  → Context Engine: user opened a file of this type
```

---

## luna-settings (System Settings)

### Role

luna-settings is the unified settings application for Mahina. It is the graphical interface for configuring the system.

### Settings Sections (v1)

```
luna-settings navigation:

  ● LUNA & AI
    ├── Presence mode settings
    ├── Memory management (view/clear LUNA memory)
    ├── Voice settings (enable TTS/STT)
    ├── Observation rules (edit observe.toml)
    └── AI model management

  ● Display
    ├── Resolution & refresh rate (per display)
    ├── Scale factor (HiDPI)
    ├── Wallpaper
    └── Night light (color temperature)

  ● Appearance
    ├── Theme (Animexcyberpunk default)
    ├── Font sizes (density: compact / regular)
    └── Cursor

  ● Desktop
    ├── Dock: pinned apps, icon size
    ├── Workspace count and names
    └── Keyboard shortcuts

  ● Network
    ├── Wi-Fi connections
    └── Wired connections

  ● Power
    ├── Battery saver threshold
    ├── Screen timeout
    └── Suspend settings

  ● Sound
    ├── Output device
    ├── Input device
    └── Volume

  ● Privacy & Security
    ├── Permission audit log viewer
    ├── AppArmor profile status
    └── Encryption status

  ● Software
    ├── Installed applications (lpkg list)
    ├── Repositories
    └── Update settings

  ● About
    ├── Mahina version
    ├── System information
    └── Hardware details
```

### Settings Storage

All settings write to TOML files in `~/.luna/config/`:

```
Settings file mapping:
  Display settings    → ~/.luna/config/display.toml
  Appearance          → ~/.luna/config/appearance.toml
  Desktop             → ~/.luna/config/shell.toml (dock, workspaces)
  Shortcuts           → ~/.luna/config/keybindings.toml
  LUNA / AI           → ~/.luna/config/luna.toml
  Observation rules   → ~/.luna/config/observe.toml
  Repositories        → ~/.luna/config/repos.toml
  Power               → ~/.luna/config/power.toml
```

---

## luna-text (Text Editor)

### Role

luna-text is the default text editor. It handles plain text, Markdown, and code files. It is not a full IDE — it is a well-designed text editor with LUNA integration.

### Features (v1)

- Syntax highlighting (common languages: C, Python, Rust, Bash, TOML, Markdown, JSON)
- Line numbers, word wrap toggle
- Find and replace
- Multiple tabs
- Cyberpunk Dark theme by default

### Features (v1.5)

- Markdown preview panel
- Git status in gutter (added/modified/deleted line indicators)
- LUNA integration: Ctrl+Shift+L → ask LUNA about selected text

### LUNA Integration

```
luna-text publishes to org.mahina.context.Files:

  FileOpened(path: string, mime_type: string)
  ActiveFileChanged(path: string)
  → Context Engine uses this for file-level DEVSHELL context
```

### Not In Scope

luna-text is **not** a full IDE. It does not include:
- LSP (Language Server Protocol) integration (v2)
- Debugger integration (v2)
- Extension system (v2)
- Terminal panel (use luna-terminal separately)

---

## luna-lock (Screen Lock)

luna-lock is specified primarily in DL-035. Key details:

```
luna-lock:

  Surface type: SYSTEM_MODAL (layer 600)
  Authentication: PAM
  Visual design:  Full-screen overlay, Luna Dark aesthetic
                  Clock display, password input field
                  LUNA Island visible but COMPACT only (no full conversation)
  Timeout:        Display turns off after 2 minutes (configurable)
  Resume:         After auth, SYSTEM_MODAL destroyed → desktop visible
```

---

## System Service Applications

### luna-netd

Thin wrapper over NetworkManager. Exposes:
- `org.mahina.network` D-Bus interface (simplified)
- luna-settings reads from here for network configuration
- luna-bar reads connectivity status from here

### luna-power

Monitors battery, handles lid events, manages suspend/shutdown:
- `org.mahina.power` D-Bus interface
- Signals luna-shell when battery is critically low
- Handles `acpi_listen` events (lid, power button)

### luna-audio (v1.5)

PipeWire session manager frontend:
- `org.mahina.audio` D-Bus interface
- luna-bar reads volume from here
- luna-settings configures audio devices via here
- Status: not in v1 — v1 uses PipeWire directly without a management daemon

---

## Application Manifest Requirements

All core applications must have a complete `luna.toml` manifest:

```toml
# Example: luna-files manifest
[package]
name        = "luna-files"
version     = "1.0.0"
description = "Mahina file manager"
license     = "MIT"

[package.app]
name        = "Files"
icon        = "luna-files"
categories  = ["Utility", "FileManager"]
exec        = "luna-files"
keywords    = ["files", "folders", "file manager"]

[package.install]
scope       = "system"  # Core apps install system-wide
```

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Browser.** Mahina v1 does not ship a browser. Firefox is the recommended third-party browser (installable via lpkg). Should Mahina ship a minimal browser as a core app? Must be a Decision Log entry.

2. **Image viewer.** No image viewer is specified. Should luna-files open images with a bundled image viewer or require a third-party app (e.g., Eye of GNOME)? Must be specified.

3. **PDF viewer.** Same question as image viewer. No PDF viewer is specified for v1.

4. **Media player.** Music and video playback — no core media player specified. Third-party (mpv, VLC) installable via lpkg. Should a minimal media player be a core app? Must be a Decision Log entry.

5. **luna-audio in v1.** v1 uses PipeWire directly. But without luna-audio, there is no unified D-Bus interface for volume control accessible to luna-bar and luna-settings. A minimal luna-audio daemon may be needed in v1 after all. Must be decided before the installer is finalized.

---

## AI Context

- Core applications are the **identity** of the desktop. They must all use LunaGUI (not GTK, not Qt) and they must all respect the visual language (Volume III/09). An inconsistently styled core app is a failure.
- luna-notif is the gatekeeper for all user-visible notifications. Do not add notification display logic to any other component — all notifications go through luna-notif.
- luna-settings writes to TOML files. When a setting changes, the relevant daemon reads the updated TOML on its next configuration check (or via a D-Bus signal if immediate effect is needed). Do not write settings to any format other than TOML.
- luna-lock is a **security boundary**. It runs as its own process, not as part of luna-shell. If luna-shell crashes while the screen is locked, luna-lock continues to protect the session. Never merge luna-lock into luna-shell.
- The core application list is intentionally minimal. Mahina is not trying to be a full desktop suite (GNOME/KDE). The core is: terminal, file manager, settings, text editor. Everything else is installable.

---

*Document: `Volume V / 05_core_applications.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume V/01_shell.md, Volume III/09_visual_language.md, DL-035*
*Informs: Volume V/06_installer.md, Volume VI/01_coding_standards.md*
