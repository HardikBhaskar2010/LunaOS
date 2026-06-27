# Mahina — Luna Graphics Protocol (LGP)
**Volume III · Chapter 1**
**Classification:** Core Architecture — Graphics Foundation
**Status:** Active · Foundational dependency for all Volume III documents

---

## Purpose

This document specifies the **Luna Graphics Protocol (LGP)**: the foundational graphics protocol of Mahina. LGP is the interface through which every graphical surface on Mahina is created, rendered, and composed.

This document is the authoritative reference for:
- What LGP is and what it is not
- The architectural relationship between LGP, LunaGUI, and the LGP compositor
- The protocol's role as a carrier of the Color Semantic Contract, Motion Vocabulary, and Animation Budget
- The boundary between LunaGUI (normal application interface) and direct-LGP (advanced application interface)
- The compositing model and surface ownership rules

This document is the entry point for Volume III. All subsequent Volume III documents (compositor, rendering pipeline, LunaGUI, animation engine, theme engine) depend on the architecture established here.

---

## Overview

LGP is Mahina's custom graphics protocol. It replaces the role that Wayland fills in conventional Linux desktops. LGP is not Wayland. LGP is not built on Wayland. No Wayland protocol is used.

LGP has two client-facing interfaces (DL-004R — hybrid graphics architecture):

**Interface A — LunaGUI toolkit:**
Standard applications use LunaGUI. LunaGUI is a widget and rendering library that uses LGP internally. Application developers interact with LunaGUI's API — they do not need to know LGP's wire format. LunaGUI is the primary path for the vast majority of Mahina applications.

**Interface B — Direct LGP:**
Advanced applications (games, video editors, custom GPU renderers) may communicate directly with the LGP compositor via the LGP protocol. Direct LGP provides full access to compositor surfaces without the widget layer overhead.

Both interfaces exist simultaneously. An application can be a LunaGUI application for its UI and request a direct LGP surface for its rendering canvas (a game's main viewport, a video player's output surface, etc.).

---

## Design Philosophy

### LGP carries Living Interface primitives as protocol-level citizens

Per `living_interface_design.md`, the Color Semantic Contract, Motion Vocabulary, and Animation Budget are system-wide constraints. LGP is the layer at which these constraints become enforced rather than advisory.

This is the critical architectural choice: **Living Interface rules are not guidelines for application developers — they are protocol-level primitives that the compositor enforces.** An application cannot send an arbitrary color to a system surface. An application cannot create a motion type not in the Motion Vocabulary. The protocol itself expresses these constraints.

Consequences:
- Every LGP surface has a **semantic color field**: applications declare the semantic meaning of a color (LUNA Green = healthy, LUNA Pink = critical), not the hex value. The compositor resolves semantic meaning to actual color values. Applications cannot set arbitrary hex values on system-semantic surfaces.
- Every animated surface transition sends a **motion class**: one of the locked Motion Vocabulary entries. The compositor's animation engine validates the class and enforces the Animation Budget ceiling.
- Applications that need truly custom colors and arbitrary animations use the **raw canvas surface** type (see Surface Types below) — but raw canvas surfaces are opt-in and cannot present themselves as system chrome surfaces.

### No application owns the compositor

In conventional Linux desktops, the compositor is typically a standalone server that clients connect to and request services from. LGP follows a similar model but with one critical difference: **no application process owns or can replace the LGP compositor.** The compositor is a Mahina system component, started by `luna-init` at Stage 5. It is not a user-replaceable shell component.

### LGP is local-only

LGP is a local Unix domain socket protocol. There is no network transport. Remote desktop / remote graphics is a future consideration and not part of LGP v1.

### Surfaces are owned by the compositor

All pixel buffers that appear on screen are owned by the compositor. Applications provide render commands or shared memory buffers; the compositor decides when and where to present them. Applications do not have direct access to the framebuffer.

---

## Architecture

### Component Relationships

```
┌──────────────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                              │
│                                                                    │
│  ┌──────────────────┐          ┌────────────────────────────────┐ │
│  │  LunaGUI app     │          │  Direct-LGP app                │ │
│  │  (widget API)    │          │  (game / video editor /        │ │
│  │                  │          │   custom renderer)             │ │
│  └────────┬─────────┘          └──────────────┬─────────────────┘ │
│           │ LunaGUI API calls                 │ LGP wire protocol  │
└───────────┼───────────────────────────────────┼────────────────────┘
            │                                   │
┌───────────▼───────────┐                       │
│     LunaGUI toolkit   │                       │
│                        │                       │
│  ┌────────────────┐   │                       │
│  │ Widget layer   │   │                       │
│  │ Layout engine  │   │                       │
│  │ Accessibility  │   │                       │
│  └────────┬───────┘   │                       │
│           │ LGP calls  │                       │
└───────────┼────────────┘                      │
            │ LGP wire protocol                 │
            ▼                                   ▼
┌────────────────────────────────────────────────────────────────────┐
│                     LGP COMPOSITOR                                  │
│                                                                      │
│  ┌────────────────┐  ┌───────────────┐  ┌────────────────────────┐  │
│  │ Surface manager│  │ Animation     │  │ Input router           │  │
│  │                │  │ engine        │  │                        │  │
│  └────────────────┘  └───────────────┘  └────────────────────────┘  │
│  ┌────────────────┐  ┌───────────────┐  ┌────────────────────────┐  │
│  │ Color resolver │  │ Z-order /     │  │ Focus manager          │  │
│  │ (semantic →    │  │ layer manager │  │                        │  │
│  │  hex value)    │  │               │  │                        │  │
│  └────────────────┘  └───────────────┘  └────────────────────────┘  │
│                                                                      │
│  Rendering pipeline (GPU backend TBD — see Open Questions)          │
└────────────────────────────────────────────────────────────────────┘
            │
            ▼
┌────────────────────────────┐
│  Linux DRM / KMS           │
│  (kernel display subsystem) │
│  Hardware GPU / framebuffer │
└────────────────────────────┘
```

### Surface Types

LGP defines the following surface types. Each type carries different constraints and permissions:

| Surface type | Who creates it | Color constraint | Motion constraint | Direct GPU access |
|---|---|---|---|---|
| `SYSTEM_CHROME` | Mahina components only | Semantic colors only | Motion Vocabulary only | No |
| `LUNA_ISLAND` | luna-ai-d only | Semantic colors only | Motion Vocabulary only | No |
| `SHELL_SURFACE` | luna-shell only | Semantic colors only | Motion Vocabulary only | No |
| `APPLICATION_WINDOW` | LunaGUI apps | Semantic colors (chrome) | Motion Vocabulary (transitions) | No |
| `CANVAS_SURFACE` | Any app (explicit request) | Unrestricted | Unrestricted | Yes (via render command) |
| `OVERLAY_SURFACE` | Privileged apps only | Semantic colors only | Motion Vocabulary only | No |

**CANVAS_SURFACE** is the escape hatch. Games, video players, and GPU-accelerated renderers request a canvas surface and render directly into it using render commands or shared GPU memory. The compositor composites the canvas onto the display but does not interpret its contents semantically.

**APPLICATION_WINDOW** is what LunaGUI provides by default. The window chrome (title bar, borders, shadows, resize handles) is `SYSTEM_CHROME`. The client area is internally a canvas but accessed via LunaGUI's rendering API rather than raw LGP commands.

---

## Protocol Design

### Transport

LGP uses a **Unix domain socket** for the control channel. The compositor listens at a well-known path:

```
/run/lgp/compositor.sock
```

Per-client connections are accepted at this socket. Each client receives a unique session identifier.

```
TODO:
Decision not yet finalized.
Reason: The exact socket path and session identifier format have not been decided.
Options:
  A: Single socket at /run/lgp/compositor.sock, with session IDs assigned per-connect.
  B: Named per-session sockets: /run/lgp/session-<id>.sock
Option A is simpler and standard. Chosen by default unless volume III compositor
document decides otherwise.
```

### Message Format

Per **DL-025**, LGP uses **TLV (Type-Length-Value)** binary framing for all wire messages. Each message consists of a 1-byte type field, a 4-byte length field, and an N-byte payload. No external serialization framework is required. This provides append-compatible message evolution, excellent hex-dump debuggability, and a minimal parser footprint.

### Core Message Types (Conceptual)

Even without a final wire format, the following message categories are architecturally decided:

**Client → Compositor:**

| Message | Effect |
|---|---|
| `LGP_CREATE_SURFACE` | Create a new surface of specified type |
| `LGP_DESTROY_SURFACE` | Release a surface and its resources |
| `LGP_COMMIT_BUFFER` | Submit rendered pixel data to the compositor |
| `LGP_REQUEST_FRAME` | Request a frame callback (vsync-aligned render signal) |
| `LGP_SET_SEMANTIC_COLOR` | Declare a semantic color state on a surface region |
| `LGP_SEND_MOTION` | Declare a motion class transition for a surface |
| `LGP_SET_GEOMETRY` | Set surface position, size, scale factor |
| `LGP_SET_LAYER` | Set Z-order layer for the surface |

**Compositor → Client:**

| Message | Effect |
|---|---|
| `LGP_FRAME_CALLBACK` | "Now is the right time to render your next frame" |
| `LGP_SURFACE_ENTER` | Surface has appeared on a display output |
| `LGP_SURFACE_LEAVE` | Surface has left a display output |
| `LGP_INPUT_EVENT` | Keyboard, pointer, or touch event for this surface |
| `LGP_COMPOSITOR_ERROR` | Protocol error notification |

### The Frame Callback Model

LGP uses a **compositor-driven frame timing** model. Applications do not render at arbitrary times and push frames. They:

1. Request a frame callback (`LGP_REQUEST_FRAME`)
2. Receive `LGP_FRAME_CALLBACK` from the compositor when it is appropriate to render
3. Render into their buffer
4. Commit the buffer (`LGP_COMMIT_BUFFER`)
5. Go to step 1

This model gives the compositor complete control over frame timing. The compositor aligns frame callbacks with the display's vsync signal. Applications that render too slowly simply drop a frame (their previous buffer is held). Applications that render too fast are throttled — they cannot commit faster than the compositor accepts.

**The Animation Budget is enforced through frame callbacks.** If an application declares a motion class transition via `LGP_SEND_MOTION`, the compositor's animation engine tracks the elapsed frames and duration. An animation that exceeds its class ceiling (`core_laws.md` Law III Animation Budget table) is automatically completed (jumped to final state) rather than allowed to run over budget.

### Semantic Color Resolution

When an application sends `LGP_SET_SEMANTIC_COLOR` on a `SYSTEM_CHROME` or `APPLICATION_WINDOW` surface region, it specifies one of the Color Semantic Contract entries:

```
LUNA_GREEN   = 0x01   // Operational, healthy, success
LUNA_PINK    = 0x02   // Critical state, error, danger
LUNA_AMBER   = 0x03   // Warning, degraded
LUNA_WHITE   = 0x04   // Neutral information, idle
LUNA_VOID    = 0x05   // Absence, offline, inactive
```

The compositor's **Color Resolver** maps these semantic codes to actual RGBA values from the active theme. This means:
- Applications using semantic colors automatically respect theming without recompile
- The Color Semantic Contract meaning (green = healthy) is enforced by the protocol — an application cannot use LUNA_GREEN for an error state without lying to the protocol
- Third-party themes may remap the hex values but never the semantic meanings

`CANVAS_SURFACE` surfaces bypass semantic color resolution entirely — they render raw RGBA.

---

## Technical Details

### Shared Memory Buffer Model

Pixel data transfer between application and compositor uses **shared memory**:

1. Application allocates a shared memory buffer (`shm_open()`)
2. Application passes the file descriptor to the compositor via the LGP socket (via `SCM_RIGHTS` ancillary message)
3. Compositor maps the buffer read-only
4. Application renders into the buffer
5. Application commits the buffer via `LGP_COMMIT_BUFFER`
6. Compositor reads the buffer, composites it, then releases it back to the application

Zero-copy semantics: the pixel data is never copied between processes. Both the application and compositor access the same physical memory pages.

```
TODO:
Decision not yet finalized.
Reason: GPU-accelerated rendering bypasses the CPU shared memory model.
For CANVAS_SURFACE with GPU rendering, the buffer may be a DMA-BUF
(kernel DMA buffer shared between GPU and CPU) rather than shm memory.
The DMA-BUF approach is required for zero-copy GPU rendering.
This must be resolved in Volume III / 02_rendering_pipeline.md.
```

### Multi-Output Support

The compositor manages multiple display outputs (monitors). Each output has:
- A display mode (resolution, refresh rate)
- A physical position in the compositor's global coordinate space
- A scale factor (for HiDPI displays)

Surfaces can span outputs or be bound to a specific output. Luna Island is bound to its primary output. Applications span their window across whichever output(s) their window geometry intersects.

### Input Routing

Input events (keyboard, pointer, touch) are routed to surfaces by the compositor's **Focus Manager**:
- Keyboard events: routed to the surface with keyboard focus (last clicked/activated surface)
- Pointer events: routed to the surface under the cursor, with hit-testing performed in compositor coordinate space
- Touch events: routed to the surface at the touch origin point

The compositor performs all input routing. Applications do not receive input events from the kernel directly — they receive compositor-delivered `LGP_INPUT_EVENT` messages only for surfaces they own.

### GPU Backend

Per **DL-026**, the GPU backend is implemented in two stages:
- **Stage 2:** Software renderer (CPU blit to dumb framebuffer). Proves the pipeline and protocol.
- **Stage 3:** Vulkan as the primary GPU renderer. OpenGL/EGL as a fallback.
The compositor communicates exclusively with the `lgp-render` abstraction layer.

The compositor interfaces with the GPU via the Linux kernel's **DRM/KMS subsystem**:
- DRM (Direct Rendering Manager) — manages GPU command submission
- KMS (Kernel Mode Setting) — manages display output mode configuration

### Protocol Versioning and Capability Negotiation

**BLOCKER 5 — Addressed here.**

Every LGP connection begins with a **handshake** before any surface operations. The handshake establishes the protocol version and negotiates capabilities. Without this, LGP v1 clients would break silently against a v2 compositor.

#### LGP_HELLO — Connection Handshake

The first message a client sends after connecting to the compositor socket is always `LGP_HELLO`:

```c
// Client → Compositor (first message, always)
typedef struct lgp_hello {
    uint32_t  magic;           // 0x4C475000  ("LGP\0") — protocol identifier
    uint16_t  version_major;   // Client's LGP major version (1 for v1)
    uint16_t  version_minor;   // Client's LGP minor version (0 for v1.0)
    uint32_t  client_flags;    // Capability flags this client supports
    char      client_name[64]; // Human-readable client name ("luna-shell", "MyApp 1.0")
} lgp_hello_t;
```

The compositor responds with `LGP_HELLO_REPLY`:

```c
// Compositor → Client (response to LGP_HELLO)
typedef struct lgp_hello_reply {
    uint32_t  magic;                   // 0x4C475000 — same identifier
    uint16_t  version_major;           // Compositor's LGP major version
    uint16_t  version_minor;           // Compositor's LGP minor version
    uint32_t  negotiated_flags;        // Intersection of client and compositor capabilities
    uint32_t  session_id;              // Unique session ID for this connection
    uint32_t  status;                  // LGP_HELLO_OK or LGP_HELLO_VERSION_REJECTED
} lgp_hello_reply_t;
```

If `status` is `LGP_HELLO_VERSION_REJECTED`, the client must disconnect. The compositor will not accept further messages from an incompatible client.

#### Version Compatibility Rules

```
Compatibility matrix:

  Client version  │  Compositor version  │  Outcome
  ─────────────────────────────────────────────────────────
  1.0             │  1.0                 │  Full compatibility
  1.0             │  1.5                 │  Full compatibility (compositor is backward-compatible)
  1.0             │  2.0                 │  Rejected if major version differs by > 1
  1.5             │  1.0                 │  Compositor uses v1.0 protocol only (negotiated_flags)
  2.0             │  1.0                 │  Rejected — major version mismatch
```

**Rule:** A compositor always accepts clients from the same major version. A compositor MAY accept clients from the previous major version (e.g., v2 compositor accepts v1 clients) if it implements a backward compatibility layer. A compositor NEVER accepts clients from a future major version.

#### Capability Flags

`client_flags` is a bitmask. Each bit represents an optional LGP capability:

```c
// LGP Capability Flags (lgp_caps.h)
#define LGP_CAP_DMA_BUF        (1 << 0)  // Client can provide DMA-BUF buffers
#define LGP_CAP_CANVAS_SURFACE (1 << 1)  // Client requests CANVAS_SURFACE access
#define LGP_CAP_DIRECT_LGP     (1 << 2)  // Client bypasses LunaGUI (direct LGP)
#define LGP_CAP_LAYER_SHELL    (1 << 3)  // Client requests privileged LAYER_SHELL+ access
#define LGP_CAP_LUNA_ISLAND    (1 << 4)  // Client requests LUNA_ISLAND surface type
#define LGP_CAP_CURSOR_SHAPE   (1 << 5)  // Client can set custom cursor shapes
#define LGP_CAP_CLIPBOARD      (1 << 6)  // Client participates in clipboard protocol
// Bits 7–31: reserved for future capabilities
```

`negotiated_flags` in the reply is `client_flags & compositor_supported_flags`. The client receives only the capabilities the compositor can actually provide.

Privileged capabilities (`LGP_CAP_LAYER_SHELL`, `LGP_CAP_LUNA_ISLAND`) are granted only if the compositor's policy allows the connecting client. The compositor validates the client identity via the socket peer credentials (`SO_PEERCRED`) against a policy file.

#### LGP Extensions

Beyond the core protocol, LGP supports extensions. An extension is a named set of additional message types that both client and compositor must agree to use.

Extension negotiation happens immediately after `LGP_HELLO`:

```c
// Client → Compositor (optional, after HELLO)
typedef struct lgp_request_extension {
    char      extension_name[64];  // e.g., "lgp_ext_clipboard_v1"
    uint16_t  version;             // Extension version requested
} lgp_request_extension_t;

// Compositor → Client
typedef struct lgp_extension_reply {
    char      extension_name[64];  // Same as request
    uint32_t  status;              // LGP_EXT_ACCEPTED or LGP_EXT_REJECTED
    uint16_t  version;             // Accepted extension version (may be lower than requested)
    uint32_t  extension_id;        // Numeric ID to use in extension messages
} lgp_extension_reply_t;
```

**Planned v1 extensions:**

| Extension name | Purpose | Target |
|---|---|---|
| `lgp_ext_clipboard_v1` | Clipboard read/write | Stage 3 |
| `lgp_ext_cursor_shape_v1` | Per-surface cursor shape | Stage 3 |
| `lgp_ext_screenshot_v1` | Privileged screen capture | Stage 4 |
| `lgp_ext_vrr_v1` | Variable refresh rate (adaptive sync) | v1.5 |
| `lgp_ext_hdr_v1` | HDR output hints | v2 |

Extensions are how LGP grows without breaking existing clients. A client that does not request an extension receives no extension messages. A compositor that does not support an extension rejects it — the client must handle the rejection gracefully and fall back to core protocol behavior.

#### Protocol Version History

| Version | Changes | Status |
|---|---|---|
| 1.0 | Initial LGP protocol | **Current — Active** |
| 1.x | Minor revisions (new capability flags, new extensions) | Future |
| 2.0 | Major revision — may introduce breaking message format changes | Future |

**ABI Stability Policy:** LGP message structures within a major version are append-only. New fields may be added at the end of a structure. Existing fields may not change type, size, or offset. Message type values (the `type` field in the header) are never reused within a major version.
- The compositor owns the DRM device (`/dev/dri/card0` or equivalent)
- No other process has direct DRM access

---

## LunaGUI — Toolkit Boundary

Per DL-004R, LunaGUI sits above LGP and provides the standard application programming interface. The boundary between LunaGUI and LGP is:

**LunaGUI is responsible for:**
- Widget primitives: buttons, inputs, labels, containers, scrollviews, dialogs
- Layout system: constraint-based or flow-based layout computation
- Rendering API: shape drawing, text rendering, image display, canvas access
- Accessibility tree: semantics for screen readers and assistive technology
- Animation API: application-level animation that maps to LGP motion classes
- Event handling: input event processing and callback dispatch

**LGP is responsible for:**
- Surface creation and lifetime
- Buffer management and commit
- Frame timing (vsync-aligned callbacks)
- Color semantic enforcement
- Motion class validation and budget enforcement
- Input event delivery to the correct surface
- Compositing all surfaces into the final display output

LunaGUI's animation API calls are translated by the LunaGUI library into `LGP_SEND_MOTION` messages with the appropriate motion class. LunaGUI does not bypass the Motion Vocabulary — it is the layer that makes the Motion Vocabulary accessible to application developers without requiring them to know the LGP wire format.

```
TODO:
Decision not yet finalized.
Reason: LunaGUI's widget API design has not been specified.
Volume III / 04_lunagui.md will define:
  - The widget primitive set
  - The layout model (constraint-based vs. flow)
  - The rendering API (canvas drawing calls)
  - The programming language binding (C API with optional Rust/Python bindings)
This document establishes the architectural boundary only.
```

---

## Living Interface Implementation Contract

This section documents how LGP implements the conceptual contract defined in `living_interface_design.md`. Every item in that document's Design Philosophy section must be traceable to an LGP mechanism.

| Living Interface requirement | LGP mechanism |
|---|---|
| Every animation maps to real system state | `LGP_SEND_MOTION` carries the motion class; the compositor validates it against the locked Motion Vocabulary |
| Every color obeys the Color Semantic Contract | `LGP_SET_SEMANTIC_COLOR` uses semantic codes; Color Resolver maps to hex; applications cannot set arbitrary hex on system surfaces |
| Animation Budget is enforced | Frame callback model + compositor animation engine tracks duration, auto-completes over-budget animations |
| Expression Layer priority — attempt color/motion before text | LGP surface type hierarchy enforces this: SYSTEM_CHROME must use semantic color and motion before any text overlay |
| Luna Island is the reference implementation | `LUNA_ISLAND` surface type is a first-class LGP surface type with its own rules |
| No decorative motion | `LGP_SEND_MOTION` requires a motion class from the locked vocabulary — there is no "decorative" class |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| LGP is the graphics protocol — no Wayland | `non_negotiables.md` | Non-Negotiable |
| Hybrid architecture: LunaGUI + direct LGP | DL-004R | Accepted |
| LGP transport: Unix domain socket at `/run/lgp/` | This document | Provisional |
| Shared memory buffer model for CPU rendering | This document | Provisional |
| LGP enforces Color Semantic Contract at protocol level | `living_interface_design.md` + this document | Accepted |
| LGP enforces Motion Vocabulary and Animation Budget | `living_interface_design.md` + this document | Accepted |
| Compositor owns DRM device exclusively | This document | Accepted |
| Frame timing: compositor-driven callback model | This document | Accepted |
| Surface types: SYSTEM_CHROME, LUNA_ISLAND, APPLICATION_WINDOW, CANVAS_SURFACE | This document | Provisional |
| LGP wire format: TLV binary, custom schema | DL-025 | Accepted |
| GPU backend: Vulkan primary, OpenGL/EGL fallback | DL-026 | Accepted |
---

## Open Questions

```
TODO:
Decision not yet finalized.
```



3. **DMA-BUF for GPU-accelerated canvas surfaces.** CPU shared memory works for LunaGUI apps. GPU-rendered CANVAS_SURFACE types need DMA-BUF. Must be resolved in `02_rendering_pipeline.md`.

4. **Multi-GPU support.** Desktop systems may have integrated + discrete GPUs. The compositor's DRM device selection for multi-GPU setups has not been specified.

5. **LunaGUI widget API.** The toolkit boundary is defined here; the API surface is deferred to `04_lunagui.md`.

6. **Accessibility.** The LGP protocol currently has no accessibility message types. Screen readers need a way to receive semantic information about surfaces. This is a v1 gap.

7. **Living Interface enforcement scope.** `living_interface_design.md` Open Question 1: does Living Interface apply only to Mahina chrome or also to third-party application windows? If only to chrome: third-party APPLICATION_WINDOW surfaces have no color/motion constraints. If to all surfaces: LunaGUI must propagate enforcement to application developers. Not yet decided.

8. **Reduced-motion accessibility mode.** `living_interface_design.md` Open Question 2: if a user enables reduced motion, how does LGP handle motion classes? The protocol currently has no reduced-motion flag on motion messages.

---

## AI Context

An AI agent implementing any Mahina graphical component must understand:

- **LGP is not Wayland.** Do not use any Wayland protocol primitives, headers, or libraries. If you find yourself including `wayland-client.h` or using `wl_surface`, stop — you are on the wrong path.
- **The compositor is a Mahina system component.** It is not a replaceable user component. It is started by `luna-init` at Stage 5. Application code never starts or restarts the compositor.
- **All pixel data goes through the compositor.** Applications never write to `/dev/dri` or the framebuffer directly. They submit buffers via LGP and the compositor decides when to present them.
- **Color Semantic Contract enforcement is at the protocol level.** If you are writing code that sets an arbitrary hex color on a `SYSTEM_CHROME` or `APPLICATION_WINDOW` surface, you are bypassing the contract. Use `LGP_SET_SEMANTIC_COLOR` with a semantic code from the locked table. `CANVAS_SURFACE` is the only exception.
- **Motion Vocabulary enforcement is at the protocol level.** `LGP_SEND_MOTION` takes a motion class from the locked vocabulary in `core_laws.md`. There is no "custom animation" class. If the required animation is not in the vocabulary, it requires the amendment process before it can be specified.
- **Animation Budget is enforced by the compositor.** Applications do not need to self-enforce the budget. The compositor will auto-complete over-budget animations. Applications should still respect the budget in their design — an animation that always gets cut off is a design error, not a compositor feature.
- **LunaGUI is the correct interface for application development.** Direct LGP should only be used for game engines, video renderers, or other components that genuinely need raw surface access. A settings panel written with direct LGP instead of LunaGUI is a misuse of the protocol.
- **The wire format is TBD.** Do not implement LGP message parsing until the wire format Decision Log entry is recorded. Use stubs or interface definitions that can be filled in once the format is decided.
- **luna-island is the reference implementation of Living Interface.** When implementing any other animated surface, its behavior should be derivable from the same LGP primitives that luna-island uses.

---

*Document: `Volume III / 01_lgp.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: living_interface_design.md, core_laws.md (Law III), non_negotiables.md, decision_log.md (DL-004R), identity.md*
*Informs: 02_rendering_pipeline.md, 03_compositor.md, 04_lunagui.md, 05_animation_engine.md, 06_theme_engine.md*
