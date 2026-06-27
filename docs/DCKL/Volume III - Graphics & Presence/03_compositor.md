# LunaOS — LGP Compositor
**Volume III · Chapter 3**
**Classification:** Core Architecture — Compositor Implementation
**Status:** Active · The compositor is the central component of the graphics stack

---

## Purpose

This document specifies the LGP compositor: the system component that owns all graphical output, manages surface lifetimes, routes input events, enforces Living Interface rules, and drives the rendering pipeline. The compositor is the most critical component in the LunaOS graphics stack. Every pixel on screen passes through it.

This document is the authoritative reference for:
- Compositor process architecture (single process, threading model)
- Surface management lifecycle
- Input routing and focus management
- Z-order and layer management
- The compositor's role as Living Interface enforcer
- Startup, shutdown, and crash recovery behavior
- The compositor's relationship to `luna-init`

---

## Overview

The LGP compositor is a single dedicated process started by `luna-init` at Stage 5 of boot. It runs as a system service in `luna-compositor.slice` (cgroup v2, highest CPU priority). It is the only process with direct access to the DRM device and therefore the only process capable of presenting pixels to the display.

The compositor performs five roles simultaneously:
1. **Protocol server** — accepts LGP connections from applications
2. **Surface manager** — tracks all active surfaces, their geometry, Z-order, and buffer state
3. **Living Interface enforcer** — validates semantic color and motion class declarations
4. **Input router** — receives raw input from kernel devices and delivers it to the correct surface
5. **Rendering orchestrator** — drives the frame clock and invokes the rendering pipeline

---

## Design Philosophy

### The compositor never fails silently

If the compositor crashes, the display goes black. There is no graceful degradation below the compositor level — without a compositor, there is no graphical output. Therefore:
- The compositor is the most carefully written component in the graphics stack
- It has no external library dependencies beyond the GPU backend and C runtime
- It is written in C (same language as `luna-init`) for predictable memory behavior
- It does not allocate memory in the hot path (frame render loop)

### The compositor is not configurable by applications

Applications cannot change compositor behavior through LGP messages. They cannot:
- Change the compositor's Z-order policy
- Request special rendering treatment outside the LGP surface type system
- Override the Animation Budget
- Add new motion classes without going through the amendment process

The compositor enforces the rules. Applications comply.

### The compositor is the reference implementation of Living Interface

`living_interface_design.md` defines Living Interface as a design discipline. The compositor is the code that makes it real. Every enforcement mechanism described in `01_lgp.md` lives in the compositor.

---

## Architecture

### Process Model

```
luna-init (PID 1)
    │
    └── lgp-compositor (Stage 5 service)
            │
            ├── Main thread (event loop)
            │     - LGP socket: accept connections, process messages
            │     - Input: read from libinput / kernel input devices
            │     - Timer: vsync callback management
            │
            ├── Render thread
            │     - Scene graph assembly
            │     - GPU command submission (via lgp-render)
            │     - Frame timing
            │
            └── (Optional future: per-client decode threads for DMA-BUF import)
```

The compositor runs two threads:
- **Main thread:** Handles all LGP protocol messages, input events, surface state changes. Single-threaded for correctness — no locking needed on surface state.
- **Render thread:** Dedicated to the GPU render pass and KMS submission. Receives a "scene snapshot" from the main thread at each frame boundary; renders it while the main thread processes the next batch of client messages.

The scene snapshot model allows the main thread and render thread to work in parallel without locking the scene graph.

### Startup Sequence

```
luna-init Stage 5:
  1. luna-init starts lgp-compositor process
  2. lgp-compositor opens DRM device (/dev/dri/card0)
  3. lgp-compositor enumerates KMS connectors and CRTCs
  4. lgp-compositor configures display mode (resolution, refresh rate)
  5. lgp-compositor initializes GPU backend (Vulkan or OpenGL/EGL)
  6. lgp-compositor allocates framebuffers
  7. lgp-compositor creates LGP socket at /run/lgp/compositor.sock
  8. lgp-compositor signals luna-init: COMPOSITOR_READY
  9. luna-init proceeds to Stage 6 (shell + LUNA Presence Engine)
```

Before step 8, no LGP clients can connect. Stage 6 components (luna-shell, luna-island) wait for the socket to appear before connecting.

Per **DL-031**, the LGP compositor signals readiness by publishing a D-Bus signal `org.lunaos.compositor.Ready`. Stage 6 components wait for this signal before connecting to the LGP socket.

### Shutdown Sequence

When `luna-init` begins system shutdown:

```
luna-init shutdown:
  1. Send SIGTERM to all Stage 6+ processes (shell, luna-ai-d)
  2. Wait for graceful exit (5 second timeout)
  3. Send SIGTERM to lgp-compositor
  4. lgp-compositor shutdown:
       a. Stop accepting new LGP connections
       b. Send LGP_COMPOSITOR_ERROR (shutdown) to all connected clients
       c. Wait for clients to disconnect (1 second timeout)
       d. Force-disconnect remaining clients
       e. Release GPU resources
       f. Release DRM device (close /dev/dri/card0)
       g. Exit
  5. luna-init proceeds with kernel-level shutdown
```

The compositor is always shut down after shell components — a blank display during shell teardown is acceptable, but a functional compositor without any shell is not useful.

### Crash Recovery & Reconnection Protocol (`lgp_ext_recovery_v1`)

If `lgp-compositor` crashes (SIGSEGV, SIGABRT, or unexpected exit), the session is not lost. LunaOS enforces a strict state-reconciliation protocol.

```
luna-init supervision:
  1. Detect compositor exit (waitpid)
  2. Log: [luna-init] [ERROR] lgp-compositor crashed (signal N)
  3. Attempt restart #1:
       a. Clean up /run/lgp/compositor.sock
       b. Start new lgp-compositor instance
       c. If successful: emit D-Bus org.lunaos.compositor.Ready
  4. If compositor crashes again within 30 seconds: enter degraded mode
       a. Do not attempt further restarts
       b. Log: [luna-init] [FATAL] lgp-compositor failed to recover
```

**Client Reconnection Sequence:**
Stage 6 processes (and all graphical apps) must implement the `lgp_ext_recovery_v1` protocol to restore the desktop seamlessly:
1. Client detects socket disconnect.
2. Client waits for `org.lunaos.compositor.Ready` via D-Bus.
3. Client reconnects to `/run/lgp/compositor.sock`.
4. Client sends `LGP_RECOVERY_BEGIN`.
5. Client resends its last known state: `LGP_CREATE_SURFACE`, `LGP_SET_GEOMETRY`, and `LGP_COMMIT_BUFFER`.
6. Client sends `LGP_RECOVERY_END`.
7. The compositor buffers all recovery requests and maps all surfaces simultaneously once the recovery window closes, preventing visual tearing.

---

## Technical Details

### Surface Manager

The compositor maintains an ordered list of surfaces. Each surface entry contains:

```c
typedef struct lgp_surface {
    uint32_t          id;           // Unique surface ID (assigned at create)
    lgp_surface_type  type;         // SYSTEM_CHROME, APPLICATION_WINDOW, etc.
    int32_t           x, y;         // Position in compositor coordinate space
    int32_t           width, height; // Dimensions
    uint32_t          layer;        // Z-order layer
    uint32_t          client_id;    // Owning client session
    lgp_buffer_t     *committed;    // Most recently committed buffer
    lgp_buffer_t     *pending;      // Buffer waiting to be committed at next frame
    bool              damaged;      // True if surface has new content this frame
    lgp_rect_t       *damage_rects; // Which sub-regions are damaged
    uint32_t          semantic_color_state; // Current semantic color if set
    bool              motion_active;        // Animation in progress
    uint32_t          motion_class;         // Active motion class
    uint64_t          motion_start_ts;      // Motion start timestamp (ms)
} lgp_surface_t;
```

Surfaces are stored in a Z-ordered array. Compositing iterates this array from index 0 (bottom) to last (top).

### Z-Order and Layer System

LGP defines compositor layers. Layer numbers determine absolute Z-order:

| Layer | Value | Who uses it | Description |
|---|---|---|---|
| `LAYER_WALLPAPER` | 0 | luna-shell | Desktop background |
| `LAYER_APPLICATION` | 100 | LunaGUI apps | Normal application windows |
| `LAYER_SHELL` | 200 | luna-shell, luna-bar | Shell chrome above apps |
| `LAYER_OVERLAY` | 300 | Privileged apps | Above shell (e.g., screenshot) |
| `LAYER_NOTIFICATION` | 400 | luna-notif | Notification cards |
| `LAYER_LUNA_ISLAND` | 500 | luna-island | Luna Island — always on top of notifications |
| `LAYER_SYSTEM_MODAL` | 600 | LUNA permission dialog | Modal system dialogs |
| `LAYER_CURSOR` | 700 | Compositor itself | Hardware/software cursor |

Within a layer, surfaces are Z-ordered by their creation time (newer surfaces on top) unless an explicit sub-order is specified.

Applications request a layer via `LGP_SET_LAYER`. The compositor validates that the requesting client is authorized to use the requested layer:
- `LAYER_SHELL` and above: only clients authenticated as LunaOS system components
- `LAYER_APPLICATION` and `LAYER_WALLPAPER`: any client

### Input Routing

The compositor reads raw input from the Linux input subsystem (via libinput or direct `evdev` reads):

Per **DL-032**, the compositor uses **libinput** for all input device management. No other process accesses `/dev/input/event*` directly.

**Pointer (mouse/trackpad) routing:**
1. Compositor maintains a global pointer position in compositor coordinate space
2. Per frame, pointer position is checked against all surface geometries (hit test)
3. The topmost surface whose geometry contains the pointer receives pointer events
4. Pointer events include: move, button press/release, scroll

**Keyboard routing:**
1. Compositor maintains a "keyboard focus surface" — the surface that receives keyboard input
2. Keyboard focus changes when:
   - A user clicks on a surface (pointer click → focus follows click)
   - A surface is raised to a layer that implies focus (LAYER_SYSTEM_MODAL steals focus)
   - A client explicitly requests focus (restricted — only system components may request focus without user action)
3. Keyboard events are delivered to the focused surface via `LGP_INPUT_EVENT`

**Input event format:**

```c
typedef struct lgp_input_event {
    uint32_t  type;       // LGP_INPUT_POINTER | LGP_INPUT_KEY | LGP_INPUT_TOUCH
    uint64_t  timestamp;  // Event timestamp (microseconds)
    union {
        struct {
            int32_t   x, y;      // Pointer position (surface-local coordinates)
            uint32_t  button;    // Button code (Linux input button codes)
            uint32_t  state;     // PRESSED | RELEASED | MOTION
            float     scroll_x, scroll_y;  // Scroll delta
        } pointer;
        struct {
            uint32_t  key;       // Linux key code
            uint32_t  state;     // PRESSED | RELEASED | REPEAT
            uint32_t  modifiers; // Active modifier keys (shift, ctrl, alt, super)
        } keyboard;
    };
} lgp_input_event_t;
```

### Living Interface Enforcement

The compositor enforces Living Interface rules at the time of LGP message processing:

**Color Semantic Enforcement:**

When a client sends `LGP_SET_SEMANTIC_COLOR` on a `SYSTEM_CHROME` or `APPLICATION_WINDOW` surface:
1. Compositor validates the semantic code is in the locked table (5 valid codes)
2. Invalid semantic code → `LGP_COMPOSITOR_ERROR` (protocol error) → client disconnected
3. Valid code → stored in `surface->semantic_color_state`
4. At render time: Color Resolver maps semantic code to active theme hex value

When a client with a `CANVAS_SURFACE` sends any color: no enforcement. Raw RGBA is accepted.

**Motion Vocabulary Enforcement:**

When a client sends `LGP_SEND_MOTION` on any non-CANVAS surface:
1. Compositor validates the motion class is in the locked Motion Vocabulary (9 valid classes)
2. Invalid motion class → `LGP_COMPOSITOR_ERROR` → client disconnected
3. Valid class → animation engine begins tracking this surface's animation
4. Animation engine enforces the Animation Budget ceiling for the class
5. If the animation exceeds its ceiling: animation engine snaps to final state, logs at DEBUG

**CANVAS_SURFACE bypass:**
`CANVAS_SURFACE` surfaces bypass all color and motion enforcement. They are the explicit escape hatch for applications that need unrestricted rendering.

### Focus Management

The compositor's focus manager tracks:
- `keyboard_focus`: Surface ID of the surface with keyboard focus
- `pointer_focus`: Surface ID of the surface the pointer is currently over
- `modal_surface`: If set, all input is directed only to this surface (system modal dialog active)

Focus changes are logged at DEBUG level. Focus steal (one surface taking focus without user action) is restricted to `LAYER_SYSTEM_MODAL` surfaces.

---

## Performance

### Memory Allocation Policy

The compositor pre-allocates its internal data structures at startup. During the render loop hot path, no `malloc()` calls are made. All surface state changes are processed through a message queue that is drained between frames, not during the render pass.

This ensures predictable frame timing — no GC pauses, no allocator latency spikes.

### CPU Affinity

```
TODO:
Decision not yet finalized.
Reason: Whether the compositor's render thread should be pinned to a specific
CPU core has not been decided. CPU affinity for the render thread could reduce
scheduling latency at the cost of less flexible CPU utilization.
This is a performance tuning question for v1 validation on reference hardware.
Related to: Luna Performance Lab (Discussion_Session_3.md) — the compositor
render thread priority and affinity may be an "Unleashed" profile feature.
```

### Compositor Log

The compositor logs to `/var/log/luna-init/runtime.log` (post-boot runtime log, not the boot log).

| Event | Level |
|---|---|
| Compositor started, display configured | INFO |
| New LGP client connected (session ID) | DEBUG |
| Client disconnected (session ID) | DEBUG |
| Surface created (type, ID) | DEBUG |
| Surface destroyed (ID) | DEBUG |
| Motion Vocabulary violation (client disconnected) | WARN |
| Color Semantic Contract violation (client disconnected) | WARN |
| Frame budget exceeded (stage, overage ms) | WARN |
| Compositor crash recovery attempt | ERROR |
| DRM device lost | FATAL |

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Hardware cursor plane | v1 | DRM cursor plane — cursor moves without re-rendering |
| Hardware overlay plane | v1.5 | DRM overlay for video surfaces — bypass GPU composite |
| Compositor-side screenshot | v1 | Privileged LGP message to capture the compositor framebuffer |
| Screen recording | v1.5 | Per-frame compositor framebuffer copy to encode pipeline |
| Remote frame protocol | v2 | Stream compositor output over network (VNC/RDP equivalent) |
| Per-output color profiles (ICC) | v1.5 | Color management for accurate display output |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **CPU affinity for render thread.** Performance tuning question for reference hardware validation.

2. **Multi-head surface spanning.** Behavior when an application window spans two monitors is not fully specified. Does the compositor clip it? Render it on both? Must be specified before multi-monitor support is declared.

6. **Per-client memory limits.** Should the compositor limit how much shared memory a single client can allocate? Relevant for sandboxed third-party applications (DL-020). Must be specified in the security architecture update.

---

## AI Context

An AI agent implementing or interacting with the LGP compositor must understand:

- The compositor is a single long-lived process started by `luna-init`. It is not a library. Do not link it into another process.
- The compositor owns `/dev/dri/card0`. No other process opens the DRM device. If you need display information (resolution, refresh rate), ask via LGP messages — do not open DRM.
- Surface types are strictly enforced. Do not request `LAYER_SHELL` or above as a third-party application. Do not request `LUNA_ISLAND` type — only `luna-ai-d` may create that surface type.
- Living Interface rules are enforced at the compositor level. `LGP_COMPOSITOR_ERROR` followed by disconnect is the consequence of a protocol violation. Write applications that comply with the Motion Vocabulary and Color Semantic Contract.
- The compositor's render loop does not allocate memory. Its message processing does. Any compositor-side code must respect this split.
- Input events arrive from the compositor, not from the kernel directly. Do not open `/dev/input/eventN` in an application — you will not receive the right events and may conflict with the compositor's input routing.
- Luna Island (`LUNA_ISLAND` surface type) is always at `LAYER_LUNA_ISLAND` (500). No application surface can appear above it except `LAYER_SYSTEM_MODAL` (600 — LUNA permission dialog only).
- The compositor has two threads: main and render. Surface state is owned by the main thread. Do not access surface state from the render thread without the scene snapshot mechanism.

---

*Document: `Volume III / 03_compositor.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, 02_rendering_pipeline.md, living_interface_design.md, core_laws.md (Law III), boot_flow.md (Stage 5), scheduler.md (luna-compositor.slice), security.md, non_negotiables.md*
*Informs: 04_lunagui.md, 05_animation_engine.md, Volume IV (luna-island compositor surface)*
