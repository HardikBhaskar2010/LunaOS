# Mahina — Shell
**Volume V · Chapter 1**
**Classification:** Core Architecture — Userland
**Status:** Canonical · This document specifies luna-shell, the Mahina desktop environment

---

## Purpose

This document specifies **luna-shell** — the process that owns the Mahina desktop. luna-shell is the first thing the user sees after the boot sequence completes, and it is the last thing running before the system shuts down.

luna-shell is responsible for:
- Creating and maintaining the wallpaper surface
- Owning the application launcher (the Mahina equivalent of a "start menu")
- Managing window focus, alt-tab, and workspace switching
- Hosting luna-bar (status bar) and luna-dock (application dock)
- Coordinating the session: login, logout, lock, shutdown

Without luna-shell, there is no desktop. Without this document, there is no canonical definition of what luna-shell does.

---

## Overview

```
luna-shell process and its surfaces:

  ┌─────────────────────────────────────────────────────────────┐
  │                        luna-shell                            │
  │                                                               │
  │  Surfaces owned:                                              │
  │   WALLPAPER surface   (layer 100)  — desktop background      │
  │   SHELL_PANEL surface (layer 200)  — luna-bar (top)          │
  │   SHELL_PANEL surface (layer 200)  — luna-dock (bottom)      │
  │   APPLICATION_WINDOW  (layer 300)  — launcher panel          │
  │   APPLICATION_WINDOW  (layer 300)  — workspace switcher      │
  │                                                               │
  │  System responsibilities:                                     │
  │   Window focus management                                     │
  │   Alt-tab switcher                                           │
  │   Workspace/virtual desktop management                       │
  │   Session control (lock, logout, shutdown)                   │
  │   Exclusion zone registration                                │
  │   Application launch dispatch                                │
  └─────────────────────────────────────────────────────────────┘

  Relationship to compositor:
    luna-shell is an LGP client — it holds surfaces like any other client.
    The compositor does NOT run shell code.
    The compositor does NOT know about workspaces or the launcher.
    luna-shell tells the compositor which surfaces to show/hide/reposition.
```

---

## Architecture

### Process Structure

```
luna-shell process (single process, multiple threads):

  Main Thread:
    LGP event loop (compositor socket)
      ├── LGP_FOCUS_CHANGED    → update focus state, notify luna-ai-d
      ├── LGP_SURFACE_CREATED  → new application window appeared
      ├── LGP_SURFACE_DESTROYED→ window closed — update task list
      ├── LGP_INPUT_EVENT      → keyboard shortcuts (alt-tab, super key)
      ├── LGP_OUTPUT_CHANGED   → display added/removed — reconfigure layout
      └── LGP_FRAME_CALLBACK   → render shell surfaces

  D-Bus Thread:
    ├── org.mahina.shell.Launch(app_id) → launch an application
    ├── org.mahina.shell.GetWindowList() → list of open windows
    ├── org.mahina.shell.FocusWindow(surface_id) → bring window to front
    ├── org.mahina.shell.MinimizeWindow(surface_id)
    ├── org.mahina.shell.CloseWindow(surface_id)
    ├── org.mahina.shell.Lock()  → trigger luna-lock
    ├── org.mahina.shell.Logout() → terminate session
    └── org.mahina.shell.Shutdown() → initiate system shutdown

  Render Thread:
    Wallpaper renderer (runs only on wallpaper update)
    luna-bar renderer (frame-driven, updates on data change)
    luna-dock renderer (frame-driven)
```

### Startup Sequence

```
luna-shell startup (called by luna-init in Stage 6):

  1. Connect to lgp-compositor via LGP socket
  2. Create WALLPAPER surface for each connected display
  3. Render default wallpaper (from ~/.luna/config/wallpaper.toml)
  4. Create SHELL_PANEL surface for luna-bar (top)
  5. Create SHELL_PANEL surface for luna-dock (bottom)
  6. Register exclusion zones with compositor:
       top:    32px (luna-bar height)
       bottom: 64px (luna-dock height)
  7. Query lpkg for installed application list → populate dock
  8. Signal luna-init: SHELL_READY
  9. Enter main event loop
```

---

## Wallpaper System

### Wallpaper Configuration

```toml
# ~/.luna/config/wallpaper.toml

[wallpaper]
mode    = "image"      # "image" | "color" | "procedural" (v1.5)
source  = "~/.luna/wallpapers/default.png"
scale   = "fill"       # "fill" | "fit" | "center" | "tile"
color   = "#0A0A0F"    # fallback color if image fails to load

# Per-display wallpaper (optional — uses default if not specified)
[[wallpaper.displays]]
output_name = "DP-1"
source      = "~/.luna/wallpapers/wide.png"
```

### Wallpaper Render Path

```
Wallpaper rendering:

  Image mode:
    Load PNG/JPEG/WebP via stb_image (or libpng + libjpeg)
    Scale to display resolution using bilinear interpolation
    Write to WALLPAPER surface shared memory buffer
    Commit via LGP_COMMIT_BUFFER

  Color mode:
    Fill surface buffer with solid color (hex from config)
    No image loading required

  Fallback (image load fails):
    Fill with #0A0A0F (Void Black from visual language spec)
    Log error: "Wallpaper load failed: [path] — using fallback"
```

---

## luna-bar (Status Bar)

### Geometry

```
luna-bar surface:
  Position: top edge of display
  Size:     display_width × 32px
  Layer:    SHELL_PANEL (200)
  Style:    Glass effect (dark navy tint, no backdrop blur in Stage 2)
```

### Layout

```
luna-bar layout (32px height):

  ┌──────────────────────────────────────────────────────────────┐
  │  [  ]  Mahina        App Name — Window Title      🌐 🔋 🔊 13:31 │
  │  ↑                   ↑                            ↑          │
  │  Super key           Active window title          System tray│
  │  (opens launcher)                                 + clock    │
  └──────────────────────────────────────────────────────────────┘

  Left zone:    Luna icon / super key affordance (32×32px)
  Center zone:  Active application name + window title (dynamic)
  Right zone:   System tray icons + clock
```

### System Tray Icons (v1)

| Icon | Source | Behavior |
|---|---|---|
| 🌐 Network | luna-netd via D-Bus | Connected/disconnected state |
| 🔋 Battery | luna-power via D-Bus | Charge % + charging indicator |
| 🔊 Volume | luna-audio via D-Bus | Volume level |
| 🕐 Clock | local system time | HH:MM format, click for date |

### Clock Format

```toml
# ~/.luna/config/bar.toml
[bar]
clock_format = "%H:%M"       # 24-hour (default)
# clock_format = "%I:%M %p"  # 12-hour alternative
show_date_on_hover = true
```

---

## luna-dock (Application Dock)

### Geometry

```
luna-dock surface:
  Position: bottom edge of display
  Size:     display_width × 64px
  Layer:    SHELL_PANEL (200)
  Style:    Glass effect
```

### Layout

```
luna-dock layout (64px height, horizontal):

  ┌───────────────────────────────────────────────────────────────┐
  │  [App1] [App2] [App3]  |  [Running1] [Running2]  |  [Trash]  │
  │   ↑ Pinned apps           ↑ Open windows             ↑ Misc  │
  └───────────────────────────────────────────────────────────────┘

  Left section:  Pinned applications (user-configured)
  Divider:       1px vertical separator
  Center section: Currently open application windows (dynamic)
  Divider:       1px vertical separator
  Right section: Utility icons (Trash, etc.)

  Each dock icon: 48×48px with 8px padding = 64px total slot
```

### Dock Icon States

```
Dock icon states:

  NOT RUNNING:  icon at 70% opacity, no indicator dot
  RUNNING:      icon at 100% opacity, small white dot below
  FOCUSED:      icon at 100% opacity, LUNA_GREEN dot below
  MINIMIZED:    icon at 60% opacity, dimmed dot below

  Hover:        icon scales to 56px (spring animation, 150ms ease-out-back)
  Click:        launch if not running / focus if running / restore if minimized
  Right-click:  context menu (Pin/Unpin, Open New Window, Close)
```

### Pinned App Configuration

```toml
# ~/.luna/config/dock.toml
[dock]
pinned = [
    "luna-terminal",
    "luna-files",
    "firefox",
    "luna-settings",
]
icon_size = 48     # px
position  = "bottom"  # "bottom" only in v1; "left"/"right" in v1.5
```

---

## Application Launcher

The launcher is the Mahina equivalent of a start menu / application grid. It is opened by:
- Pressing the **Super key**
- Clicking the **Luna icon** in luna-bar
- Calling `org.mahina.shell.OpenLauncher()` via D-Bus

### Launcher Layout

```
Launcher panel (full-height left panel — 380px wide):

  ┌──────────────────────────────────────────┐
  │  🔍  Search applications...              │  ← Search input (autofocused)
  ├──────────────────────────────────────────┤
  │  Recent                                  │
  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
  │  │  []  │ │  []  │ │  []  │ │  []  │   │  ← 4 most recently used apps
  │  │ Term │ │ Code │ │ Fire │ │ Files│   │
  │  └──────┘ └──────┘ └──────┘ └──────┘   │
  ├──────────────────────────────────────────┤
  │  All Applications              [A–Z ▼]  │
  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
  │  │      │ │      │ │      │ │      │   │  ← App grid (alphabetical)
  │  └──────┘ └──────┘ └──────┘ └──────┘   │
  │  ...                                     │
  ├──────────────────────────────────────────┤
  │  [⏻ Power]  [🔒 Lock]  [⚙ Settings]    │  ← Session controls
  └──────────────────────────────────────────┘

  Width:    380px (fixed)
  Height:   display_height - bar_height - dock_height
  Position: left edge of display (slides in from left)
  Layer:    APPLICATION_WINDOW (300) — above shell panels
  Animation: slide-in from left, 280ms ease-out-quint
```

### Launcher Search

Search queries are matched against:
1. Application name (exact + fuzzy)
2. Application description
3. Application keywords (from the app's `luna.toml` manifest)

```python
def search_apps(query: str, app_list: list) -> list:
    """
    Returns apps sorted by relevance:
    1. Exact name match (highest)
    2. Name starts with query
    3. Name contains query
    4. Description/keyword match (lowest)
    """
    ...
```

Pressing **Enter** on a search result launches the top match. Pressing **Escape** closes the launcher.

### Application Launch Dispatch

```c
// Launching an application from luna-shell

void shell_launch_app(const char *app_id) {
    // 1. Look up app_id in the lpkg application registry
    //    (/var/lib/lpkg/apps/<app_id>/luna.toml)
    luna_app_manifest_t *manifest = lpkg_get_manifest(app_id);
    if (!manifest) {
        log_error("launch: app not found: %s", app_id);
        return;
    }

    // 2. Fork + exec the application binary
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid();  // new session — app is independent of shell
        execve(manifest->exec_path, manifest->exec_args, minimal_env);
        _exit(1);  // exec failed
    }

    // 3. Record the launch (for Recent apps list)
    record_app_launch(app_id);

    // 4. Monitor for the application's surface to appear
    //    (LGP_SURFACE_CREATED event with matching client_name)
    register_launch_monitor(app_id, pid, timeout_ms = 10000);
}
```

---

## Window Management

luna-shell manages the application window list and handles user-initiated window operations.

### Window State Tracking

```c
typedef struct shell_window {
    uint32_t  surface_id;       // LGP surface ID
    char      app_id[128];      // from lpkg registry
    char      title[256];       // from LGP_SURFACE_TITLE_CHANGED
    pid_t     pid;              // owning process PID
    uint32_t  workspace_id;     // which workspace this window is on
    bool      is_focused;       // keyboard focus
    bool      is_minimized;
    bool      is_maximized;
    bool      is_fullscreen;
    uint32_t  z_order;          // position in layer (updated on focus)
} shell_window_t;
```

### Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Super` | Open/close launcher |
| `Super + D` | Show desktop (minimize all windows) |
| `Super + L` | Lock screen |
| `Super + Q` | Close focused window |
| `Super + ↑` | Maximize focused window |
| `Super + ↓` | Restore/minimize focused window |
| `Super + ←/→` | Tile window left/right (50% width) |
| `Super + 1–9` | Switch to workspace N |
| `Alt + Tab` | Cycle through open windows (alt-tab switcher) |
| `Alt + F4` | Close focused window |
| `Alt + F10` | Toggle maximize |

### Alt-Tab Switcher

```
Alt-tab switcher overlay:

  ┌──────────────────────────────────────────────────────────┐
  │                                                            │
  │   ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐               │
  │   │      │  │      │  │      │  │      │               │
  │   │  []  │  │  []  │  │  []  │  │  []  │               │
  │   │      │  │      │  │      │  │      │               │
  │   └──────┘  └──────┘  └──────┘  └──────┘               │
  │   Terminal   Firefox   Code     Files                    │
  │      ↑                                                   │
  │  Selected (highlighted border)                          │
  │                                                            │
  └──────────────────────────────────────────────────────────┘

  Position: centered on display
  Layer:    APPLICATION_WINDOW (300)
  Thumbnails: live previews of each window (v1.5) or app icons (v1)
  Navigation: Alt+Tab (next), Alt+Shift+Tab (previous), release Alt (confirm)
```

---

## Workspace Management

Mahina v1 supports **static workspaces** — a fixed number of virtual desktops.

```
Workspace model (v1):

  Count:    4 workspaces (user-configurable, 1–9)
  Switching: Super+1 through Super+4
  Moving windows: Super+Shift+1 through Super+Shift+4

  Workspace state:
    Each workspace has its own window list.
    The compositor only composites surfaces on the active workspace
    (inactive workspace windows are unmapped — surface exists, not rendered).

  Multi-display:
    Each display has its own active workspace index.
    Workspace 1 on display 1 is independent of workspace 1 on display 2.
    (v1 behavior — may be revised for v2)
```

```toml
# ~/.luna/config/workspaces.toml
[workspaces]
count = 4
names = ["Main", "Browser", "Terminal", "Media"]
```

---

## Session Management

luna-shell owns session lifecycle operations:

### Lock

```
Lock sequence:

  Trigger: Super+L, luna-bar lock button, lid close (luna-power signal)

  1. luna-shell calls org.mahina.luna.PrepareForLock() on luna-ai-d
     (luna-ai-d saves context, suspends observation)
  2. luna-shell spawns luna-lock process
  3. luna-lock creates SYSTEM_MODAL surface (layer 600)
     (covers everything — luna-shell still running underneath)
  4. luna-lock authenticates user (PAM)
  5. On successful auth:
     luna-lock destroys its SYSTEM_MODAL surface
     luna-lock exits
     luna-ai-d resumes observation
     luna-shell resumes normal operation
```

### Logout

```
Logout sequence:

  1. luna-shell sends SIGTERM to all application windows (ordered: user apps first)
  2. Waits up to 5 seconds for each app to exit gracefully
  3. SIGKILL any remaining processes
  4. luna-ai-d shutdown (summarization pass)
  5. luna-shell destroys all its surfaces
  6. luna-shell exits
  7. luna-init detects shell exit → proceeds with full shutdown or next user session
```

### Shutdown / Reboot

```
Shutdown trigger:

  luna-shell calls org.mahina.lunaInit.Shutdown() D-Bus method
  (luna-init owns the shutdown — shell requests it)
  luna-init proceeds with the Stage 7 shutdown sequence (Volume II/04)
```

---

## Failure Modes

```
luna-shell failure handling:

  luna-bar crash (child process):
    → luna-shell detects via SIGCHLD
    → Restart luna-bar after 1 second
    → No visible disruption to applications

  luna-dock crash:
    → Same as luna-bar — restart after 1 second

  Wallpaper surface lost:
    → LGP_SURFACE_LOST event received
    → Re-create WALLPAPER surface
    → Re-render wallpaper
    → User sees brief compositor background color (~100ms)

  luna-shell crash:
    → luna-init detects via SIGCHLD
    → luna-init restarts luna-shell immediately
    → All application windows remain alive (they are separate processes)
    → Shell surfaces re-created on restart
    → User sees: wallpaper briefly disappears + reappears (~500ms)
    → Applications continue running — no work lost

  Compositor crash:
    → All LGP connections lost
    → luna-shell exits (LGP socket read failure)
    → luna-init restarts compositor, then luna-shell
    → Applications also exit (their LGP connections die)
    → Full session restart required
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| luna-shell is a separate process from the compositor | Volume II/01, DL-004R | ✅ Accepted |
| luna-bar height: 32px | This document | ✅ Accepted |
| luna-dock height: 64px | This document | ✅ Accepted |
| Launcher opens on Super key | This document | ✅ Accepted |
| 4 default workspaces | This document | 🧪 Experimental |
| Workspace model: static count (v1) | This document | ✅ Accepted |
| Dock position: bottom only in v1 | This document | ✅ Accepted |
| Shell controls lock/logout/shutdown dispatch | This document | ✅ Accepted |
| Alt-tab thumbnails: app icons in v1, live preview in v1.5 | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Dynamic workspaces.** v1 uses a fixed workspace count. Should v2 support dynamic workspace creation (create on demand, destroy when empty)? Many modern desktops (GNOME, Sway) use dynamic workspaces. Must be a Decision Log entry for v2.

2. **Dock on left/right.** Dock position is bottom-only in v1. Left/right dock positions are planned for v1.5. The geometry calculation for left/right docks (vertical layout, exclusion zones on left/right edges) must be specified before implementation.

3. **Notification center.** luna-bar's right zone currently shows tray icons. Should there be a notification center (click to see notification history) accessible from luna-bar? This involves luna-notif integration. Must be designed before luna-notif is specified.

4. **Multi-display workspace model.** Each display has independent workspaces in v1. This means workspace 1 on display 1 and workspace 1 on display 2 are separate. Should the model change to "global workspaces span all displays"? Common alternatives: unified workspaces, independent per-display workspaces, or span-primary-only. Must be a Decision Log entry.

5. **Application window thumbnails (alt-tab).** v1 alt-tab shows app icons. v1.5 should show live window previews. Live previews require the compositor to provide surface snapshots (similar to the Vision Module's capture API). The alt-tab compositor extension must be specified.

---

## AI Context

- luna-shell **does not render application content**. It renders the shell chrome (wallpaper, bar, dock, launcher) and tells the compositor what to do with application windows via LGP protocol messages. Applications render themselves.
- The launcher is an APPLICATION_WINDOW surface, not a SHELL_PANEL. This means it can be overlaid by SYSTEM_OVERLAY surfaces (luna-island, toasts) above it. The layer ordering is intentional.
- luna-shell is responsible for keyboard shortcut interception. When luna-shell holds keyboard grab for a shortcut (Super key, Alt+Tab), those keys are not delivered to the focused application. The grab must be released after the shortcut is handled.
- luna-shell crash recovery is designed to leave applications alive. The applications are separate processes with independent LGP connections. When luna-shell dies, applications lose their compositor connection too — but this is because the compositor crashes alongside, not because of luna-shell itself. A luna-shell-only crash (not compositor crash) does not kill applications.
- Workspace switching is done by luna-shell instructing the compositor to unmap surfaces of inactive workspace windows. The compositor does not know about workspaces — it just maps/unmaps surfaces as instructed.

---

*Document: `Volume V / 01_shell.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume II/01_architecture_overview.md, Volume II/02_boot_flow.md, Volume III/03_compositor.md, Volume III/08_window_objects.md, DL-004R, DL-035*
*Informs: Volume V/05_core_applications.md, Volume V/06_installer.md*
