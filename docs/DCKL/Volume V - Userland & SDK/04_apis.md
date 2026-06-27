# Mahina — Public APIs
**Volume V · Chapter 4**
**Classification:** Core Architecture — Userland
**Status:** Canonical · This document is the authoritative reference for all public Mahina API surfaces

---

## Purpose

This document defines every **public API** that Mahina exposes to application developers. An API is "public" if it is designed to be used by third-party applications. Internal-only APIs (e.g., luna-ai-d's internal component interfaces) are not documented here.

This document answers: "As an application developer, what can I call to interact with Mahina?"

---

## Overview

```
Mahina public API surface — three layers:

  LAYER 1: LGP Protocol (graphics, input, display)
    ← C API wrapping the LGP socket protocol
    ← Used by: any application that renders graphics
    ← Required for: creating windows, receiving input

  LAYER 2: D-Bus APIs (system services)
    ← Standard D-Bus interfaces to Mahina system services
    ← Used by: applications that need system integration
    ← Optional: apps work without using D-Bus at all

  LAYER 3: LUNA SDK (high-level C/Python library)
    ← Wraps LGP + D-Bus in a developer-friendly API
    ← Used by: most third-party Mahina applications
    ← Volume V/08 specifies the full SDK
```

---

## Layer 1: LGP API

The LGP API is the foundational graphics API. All window creation, rendering, and input handling goes through LGP. See `Volume III / 01_lgp.md` for the full protocol specification. This section summarizes the public C API.

### Surface Management

```c
// Connect to compositor
lgp_connection_t* lgp_connect(const char *app_name, const char *app_version);
void              lgp_disconnect(lgp_connection_t *conn);

// Create a window surface
lgp_surface_t* lgp_create_surface(
    lgp_connection_t *conn,
    lgp_surface_type_t type,    // LGP_SURFACE_APPLICATION_WINDOW for normal apps
    int x, int y,               // initial position (compositor may override)
    int width, int height,
    uint32_t flags              // LGP_SURFACE_FLAG_*
);

void lgp_destroy_surface(lgp_surface_t *surface);

// Resize
void lgp_resize_surface(lgp_surface_t *surface, int width, int height);

// Surface title
void lgp_set_surface_title(lgp_surface_t *surface, const char *title);

// Surface states
void lgp_request_maximize(lgp_surface_t *surface);
void lgp_request_minimize(lgp_surface_t *surface);
void lgp_request_fullscreen(lgp_surface_t *surface);
void lgp_request_restore(lgp_surface_t *surface);
```

### Buffer and Rendering

```c
// Shared memory buffer
lgp_buffer_t* lgp_create_shm_buffer(lgp_surface_t *surface, int width, int height);
void*         lgp_buffer_data(lgp_buffer_t *buffer);   // returns pixel data pointer
void          lgp_buffer_release(lgp_buffer_t *buffer);

// GPU buffer (DMA-BUF, v2)
lgp_buffer_t* lgp_create_dmabuf(lgp_surface_t *surface, int dmabuf_fd,
                                  int width, int height, uint32_t format);

// Commit rendered buffer
void lgp_commit_buffer(lgp_surface_t *surface, lgp_buffer_t *buffer,
                        lgp_damage_region_t *damage);  // damage = dirty region hint

// Frame callback (synchronize with compositor refresh)
void lgp_request_frame_callback(lgp_surface_t *surface,
                                 lgp_frame_callback_fn callback, void *user_data);
```

### Input Events

```c
// Event loop
lgp_event_t* lgp_next_event(lgp_connection_t *conn, int timeout_ms);
void         lgp_event_free(lgp_event_t *event);

// Event types
typedef enum {
    LGP_EVENT_KEY_PRESS,
    LGP_EVENT_KEY_RELEASE,
    LGP_EVENT_POINTER_MOTION,
    LGP_EVENT_POINTER_BUTTON,
    LGP_EVENT_POINTER_SCROLL,
    LGP_EVENT_TOUCH_DOWN,
    LGP_EVENT_TOUCH_UP,
    LGP_EVENT_TOUCH_MOTION,
    LGP_EVENT_FOCUS_ENTER,      // keyboard focus gained
    LGP_EVENT_FOCUS_LEAVE,      // keyboard focus lost
    LGP_EVENT_SURFACE_CLOSE,    // user closed window (X button)
    LGP_EVENT_SURFACE_RESIZE,   // compositor requested resize
    LGP_EVENT_OUTPUT_CHANGED,   // display configuration changed
    LGP_EVENT_FRAME_CALLBACK,   // frame timing event
    LGP_EVENT_THEME_CHANGED,    // active theme changed
} lgp_event_type_t;
```

### Display Information

```c
// Get all connected displays
lgp_output_list_t* lgp_get_outputs(lgp_connection_t *conn);

typedef struct lgp_output_info {
    char     name[64];          // e.g. "DP-1", "HDMI-2"
    int      width, height;     // resolution in pixels
    int      refresh_hz;        // refresh rate
    float    scale_factor;      // HiDPI scale (1.0, 1.5, 2.0...)
    bool     is_primary;
} lgp_output_info_t;

// Get workspace geometry (respects exclusion zones)
lgp_workspace_geometry_t lgp_get_workspace_geometry(lgp_connection_t *conn,
                                                      const char *output_name);
```

---

## Layer 2: D-Bus APIs

### org.lunaos.shell — Desktop Shell

```
Service:    org.lunaos.shell
Object:     /org/lunaos/shell

Methods:
  Launch(app_id: string) → void
    Launch an installed application by its lpkg package name.
    
  GetWindowList() → array<WindowInfo>
    Returns list of all open windows with their surface IDs, titles, PIDs.
    
  FocusWindow(surface_id: uint32) → void
    Bring the specified window to focus.
    
  MinimizeWindow(surface_id: uint32) → void
  CloseWindow(surface_id: uint32) → void
    Request the window to close (sends SURFACE_CLOSE event to the window).
    
  OpenLauncher() → void
    Open the application launcher panel.
    
  Lock() → void
    Lock the screen immediately.
    
  GetActiveWindow() → WindowInfo
    Returns info about the currently focused window.

Signals:
  WindowOpened(surface_id: uint32, app_id: string, title: string)
  WindowClosed(surface_id: uint32, app_id: string)
  WindowFocusChanged(surface_id: uint32)
  WorkspaceChanged(workspace_id: uint32)
```

### org.lunaos.luna — AI Presence

```
Service:    org.lunaos.luna
Object:     /org/lunaos/luna

Methods:
  GetMode() → string
    Returns current LUNA mode: "AMBIENT"|"DEVSHELL"|"FOCUS"|"STUDY"|"CREATIVE"|"GAMING"
    
  GetContext() → dict
    Returns public context snapshot (see Volume IV/03 for fields).
    
  OpenConversation() → void
    Open luna-island in FULL_CONVERSATION state (if permitted).
    
  OpenConversationWithContext(context: dict) → void
    Open conversation with pre-loaded context.
    Used by luna-terminal LUNA Assist.
    
  SendObservation(type: string, data: dict) → void
    Allow an app to send an observation to the Presence Engine.
    Requires: "observe_publish" capability in luna.toml.

Signals:
  ModeChanged(new_mode: string, old_mode: string)
  ExpressionChanged(expression_type: string, color: string, duration_ms: uint32)
  TokenReceived(token: string, is_final: bool, turn_id: uint32)
```

### org.lunaos.pkg — Package Manager

```
Service:    org.lunaos.pkg
Object:     /org/lunaos/pkg

Methods:
  Install(package_id: string, scope: string) → operation_id: uint32
  Remove(package_id: string, purge: bool) → operation_id: uint32
  Upgrade(package_id: string) → operation_id: uint32
  UpgradeAll() → operation_id: uint32
  
  GetPackageInfo(package_id: string) → PackageInfo dict
  GetInstalledPackages() → array<PackageInfo>
  Search(query: string) → array<PackageInfo>

Signals:
  OperationProgress(op_id: uint32, phase: string, percent: uint32)
  OperationCompleted(op_id: uint32, result: string, error_msg: string)
  PackageInstalled(package_id: string)
  PackageRemoved(package_id: string)
```

### org.lunaos.notify — Notifications

```
Service:    org.lunaos.notify
Object:     /org/lunaos/notify
Standard:   Compatible with freedesktop.org Notification spec (subset)

Methods:
  Notify(
      app_name:       string,
      replaces_id:    uint32,    # 0 = new notification
      app_icon:       string,
      summary:        string,
      body:           string,
      actions:        array<string>,  # ["action-id", "Action Label", ...]
      hints:          dict,
      expire_timeout: int32      # ms, -1 = never, 0 = default
  ) → notification_id: uint32
  
  CloseNotification(notification_id: uint32) → void
  GetCapabilities() → array<string>
  GetServerInformation() → (name, vendor, version, spec_version)

Signals:
  NotificationClosed(notification_id: uint32, reason: uint32)
  ActionInvoked(notification_id: uint32, action_key: string)
```

### org.lunaos.theme — Theme Engine

```
Service:    org.lunaos.theme
Object:     /org/lunaos/theme

Methods:
  GetActiveTheme() → ThemeInfo dict
    Returns: theme name, variant ("dark"|"light"), all color tokens as key-value pairs.
    
  GetColor(token_name: string) → string
    Returns hex color for a semantic token: "LUNA_GREEN", "surface_dark", etc.
    
  GetFontInfo() → FontInfo dict
    Returns: display_font, reading_font, base_size, scale_factor.

Signals:
  ThemeChanged(theme_name: string, variant: string)
    Emitted when the active theme changes.
    Applications should re-query all color tokens on this signal.
```

### org.lunaos.power — Power Management

```
Service:    org.lunaos.power
Object:     /org/lunaos/power

Methods:
  GetBatteryInfo() → BatteryInfo dict
    Returns: present, percent, charging, time_to_empty_minutes.
    
  Shutdown() → void
  Reboot() → void
  Suspend() → void
    All require authentication via the LUNA permission system.

Signals:
  BatteryChanged(percent: uint32, charging: bool)
  PowerStateChanged(state: string)  # "on_battery" | "on_ac" | "low_battery"
```

---

## Layer 2: Context API (Application Self-Report)

Applications may publish context information to LUNA's Context Engine to improve observation quality:

```
Service:    org.lunaos.context
Object:     /org/lunaos/context/<app_name>

Methods (published BY application, consumed BY luna-ai-d):

  SetActiveFile(file_path: string) → void
    Publish the currently active file.
    Requires: "observe_active_file = true" in observe.toml for this app.
    
  SetActiveDocument(title: string, page: uint32, total_pages: uint32) → void
    For document viewers — publish what the user is reading.
    
  SetProjectContext(project_name: string, context_type: string) → void
    Publish the current project context.
    context_type: "coding" | "writing" | "design" | "research"
```

---

## API Versioning

All D-Bus interfaces include a version method:

```
Every interface implements:
  GetVersion() → (major: uint32, minor: uint32, patch: uint32)
  
Current versions (v1):
  org.lunaos.shell:   1.0.0
  org.lunaos.luna:    1.0.0
  org.lunaos.pkg:     1.0.0
  org.lunaos.notify:  1.0.0
  org.lunaos.theme:   1.0.0
  org.lunaos.power:   1.0.0
  org.lunaos.context: 1.0.0
```

---

## API Stability Promise

```
API stability contract for Mahina v1.x:

  STABLE (breaking changes require major version bump):
    - All method signatures in Layer 2 D-Bus APIs
    - All event types in the LGP C API
    - All struct layouts in the LGP C API

  UNSTABLE (may change in minor versions):
    - Internal-only APIs (luna-ai-d component interfaces)
    - luna.toml manifest schema additions (additions only, no removals)
    - D-Bus signal payloads (fields may be added, not removed)

  EXPERIMENTAL (explicitly unstable, expect breakage):
    - org.lunaos.context (application context publishing)
    - Any API marked with [EXPERIMENTAL] in documentation
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| LGP is the graphics protocol — no Wayland | non_negotiables.md | ✅ Accepted |
| D-Bus for system service APIs | Volume II/07 | ✅ Accepted |
| Notification API compatible with freedesktop spec | This document | ✅ Accepted |
| Application context publishing via org.lunaos.context | This document | 🧪 Experimental |
| API stability promise: stable for v1.x | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **LGP C API formalization.** The LGP protocol is specified in Volume III/01, but the C header API (lgp.h) has not been written. The API signatures in this document are drafts. The actual header file must be the canonical source.

2. **Accessibility API.** AT-SPI2 (DL-040) provides an accessibility bridge. The D-Bus interface for AT-SPI2 is a standard — but the Mahina-specific extensions (if any) must be specified.

3. **IPC for non-D-Bus apps.** Some applications (games, legacy software) may not use D-Bus. Is there a simpler IPC mechanism (Unix socket, shared memory) for low-overhead notifications? Must be specified.

4. **API authentication.** D-Bus methods like `Shell.Shutdown()` must require authentication. Who is allowed to call these? Currently: any process running as the logged-in user. Should there be capability-based access control on D-Bus method calls? Must be a Decision Log entry.

5. **WebSocket bridge for web apps.** Should Mahina provide a WebSocket bridge that web applications (running in the browser) can use to access D-Bus APIs? This would enable browser-based apps to integrate with LUNA's presence system. Must be a Decision Log entry.

---

## AI Context

- The three-layer API model (LGP, D-Bus, SDK) is intentional. Applications don't have to use all three. A game uses only LGP. A settings panel uses D-Bus only (via SDK). A full application uses all three.
- The `org.lunaos.context` API is experimental — it allows apps to publish context to LUNA but the exact fields and behavior may change. Do not build critical features on top of it in v1.
- The Notification API is freedesktop-compatible. This means existing Linux applications that use libnotify will work on Mahina without modification — they send to `org.freedesktop.Notifications` and luna-notif implements that interface.
- `Shell.CloseWindow()` sends a close request to the window — it does not force-kill the application. The application must handle the `LGP_EVENT_SURFACE_CLOSE` event and clean up gracefully. Force-kill is only available to the shell itself, not to third-party callers.
- All API signatures in this document are drafts until the C headers and D-Bus introspection files are generated and published. When implementation begins, the headers become canonical.

---

*Document: `Volume V / 04_apis.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume III/01_lgp.md, Volume II/07_ipc.md, Volume IV/00_luna_runtime.md*
*Informs: Volume V/08_sdk.md, Volume VI/02_ai_coding_guidelines.md*
