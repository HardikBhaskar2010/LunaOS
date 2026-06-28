# LGP Compositor — Directory Structure
**Mahina OS — `src/lgp-compositor/`**
**Companion to:** `LGP_IMPLEMENTATION_PLAN.md`

This layout is sized for the full Volume III scope (M0 through M10 in the implementation plan), not just the first four tasks — so that later milestones land in directories that already exist with an already-understood purpose, rather than requiring a restructure. Every directory below maps to a specific DCKL authority document. Naming mirrors the module names `01_lgp.md` and `02_rendering_pipeline.md` already use (`lgp-render`, etc.) so that compositor source code and documentation cross-reference each other without translation.

```
src/lgp-compositor/
├── main.c                      # Entry point: epoll loop, startup/shutdown sequence
│                                #   Authority: 03_compositor.md §Startup/Shutdown Sequence
├── Makefile.inc                # Included by root Makefile; mirrors luna-init's build pattern
│
├── drm/                        # Direct Rendering Manager — device + connector layer
│   │                           #   Authority: 02_rendering_pipeline.md §DRM/KMS Interface
│   ├── device.c/.h             # open()/close() of /dev/dri/cardN, capability query
│   ├── connector.c/.h          # Connector enumeration, mode list retrieval
│   └── drm_internal.h          # Shared internal types not exposed outside drm/
│
├── kms/                        # Kernel Mode Setting — display configuration + presentation
│   │                           #   Authority: 02_rendering_pipeline.md §DRM/KMS Interface,
│   │                           #              03_compositor.md §Startup Sequence steps 3-4
│   ├── crtc.c/.h                # CRTC/connector association, atomic mode commit
│   ├── dumb_buffer.c/.h         # Dumb framebuffer alloc/map/free (Stage 2 software path)
│   └── page_flip.c/.h           # vblank-synchronized presentation, page flip event handling
│
├── render/                     # GPU abstraction layer — "lgp-render" from the DCKL
│   │                           #   Authority: 02_rendering_pipeline.md §GPU Abstraction Layer
│   ├── render_target.c/.h       # render_target_create() — framebuffer/texture targets
│   ├── compositing.c/.h         # render_surface(), render_composite() — Z-order blend pass
│   ├── present.c/.h             # render_present() — hands off to kms/page_flip.c
│   ├── software/                # Stage 2 backend (DL-026): CPU blit to dumb buffer
│   │   └── sw_blit.c/.h
│   └── gpu/                     # Stage 3 backend (DL-026) — not implemented before M10
│       ├── vulkan/              # Primary GPU backend
│       └── egl/                 # OpenGL/EGL fallback for non-Vulkan hardware
│
├── ipc/                        # LGP Unix socket server + per-client connection state
│   │                           #   Authority: 01_lgp.md §Protocol Design / Transport,
│   │                           #              03_compositor.md §Crash Recovery
│   ├── socket_server.c/.h       # bind/listen/accept at /run/lgp/compositor.sock
│   ├── client.c/.h               # lgp_client_t lifecycle, SO_PEERCRED extraction
│   └── recovery.c/.h             # lgp_ext_recovery_v1 — populated at M9, stubbed earlier
│
├── protocol/                   # LGP wire format — TLV framing, handshake, message catalogue
│   │                           #   Authority: 01_lgp.md (DL-025), full document
│   ├── tlv.c/.h                  # Generic TLV encode/decode, bounds checking
│   ├── hello.c/.h                 # LGP_HELLO / LGP_HELLO_REPLY handshake + version matrix
│   ├── caps.c/.h                   # Capability flag table, negotiation, policy hook
│   ├── extensions.c/.h             # Extension registration (lgp_ext_clipboard_v1, etc.)
│   └── messages/                  # One file per message family, added as each is implemented
│       ├── surface_messages.c/.h    # LGP_CREATE_SURFACE / LGP_DESTROY_SURFACE — M4
│       ├── buffer_messages.c/.h     # LGP_COMMIT_BUFFER / LGP_REQUEST_FRAME — M4/M5
│       ├── color_messages.c/.h      # LGP_SET_SEMANTIC_COLOR — M6, blocked on Color Contract resolution
│       ├── motion_messages.c/.h     # LGP_SEND_MOTION — M7
│       └── input_messages.c/.h      # LGP_INPUT_EVENT — M8
│
├── client/                     # Compositor-side model of what each connected client owns
│   │                           #   Authority: 13_component_ownership.md §Window Lifecycle
│   ├── session.c/.h              # Maps session_id -> lgp_client_t -> owned surface IDs
│   └── policy.c/.h                # Per-client capability grants (LAYER_SHELL, LUNA_ISLAND, etc.)
│
├── scene/                      # Surface manager — the authoritative Z-ordered surface list
│   │                           #   Authority: 03_compositor.md §Surface Manager, §Z-Order
│   ├── surface.c/.h              # lgp_surface_t lifecycle (M4)
│   ├── zorder.c/.h                 # Layer table + insertion ordering — BLOCKED on §2.1-B
│   │                                #   (LGP_IMPLEMENTATION_PLAN.md §2.1-B) until the
│   │                                #   03_compositor.md vs 08_window_objects.md layer
│   │                                #   numbering conflict is resolved
│   └── snapshot.c/.h               # Scene-snapshot handoff to the render thread (M5+)
│
├── backend/                    # GPU backend selection glue — chooses render/gpu/vulkan vs egl
│   └── backend_select.c/.h       # Vulkan 1.1+ capability probe, EGL fallback decision (M10)
│
├── input/                      # libinput integration — DL-032
│   │                           #   Authority: 03_compositor.md §Input Routing
│   ├── libinput_context.c/.h     # libinput context lifecycle, /dev/input/event* ownership
│   ├── pointer.c/.h                # Hit-testing, pointer focus tracking
│   └── keyboard.c/.h               # Keyboard focus tracking, modifier state
│
├── cursor/                     # Hardware/software cursor rendering
│   │                           #   Authority: 03_compositor.md §Layer System (LAYER_CURSOR=700)
│   └── cursor.c/.h                # Cursor plane management (hardware) or composited fallback
│
├── damage/                     # Damage tracking — which screen regions changed this frame
│   │                           #   Authority: 02_rendering_pipeline.md §Damage tracking
│   └── damage_region.c/.h        # Per-surface damage rect accumulation/clearing
│
├── animation/                  # Animation engine — Motion Vocabulary + Animation Budget
│   │                           #   Authority: core_laws.md Law III, 02_rendering_pipeline.md
│   │                           #              §Animation Engine Integration — M7, blocked on
│   │                           #              nothing protocol-level but depends on M6 color work
│   ├── motion_class.c/.h          # The 9 locked motion classes + easing functions
│   └── budget.c/.h                  # Per-class ceiling enforcement, auto-complete-on-overage
│
├── util/                       # Shared utilities with no DCKL chapter of their own
│   ├── ring_buffer.c/.h           # Bounded queues (e.g., the message-queue-drained-between-frames
│   │                                #   pattern from 03_compositor.md §Memory Allocation Policy)
│   └── bounds.c/.h                  # Shared bounds-checking helpers for protocol/ and ipc/
│
├── logging/                    # Thin wrapper around the runtime.log format
│   │                           #   Authority: 03_compositor.md §Compositor Log
│   └── log.c/.h                   # Reuses the luna-init log line format for consistency
│
├── config/                     # Compositor-side configuration (not theme — see note below)
│   └── args.c/.h                  # CLI flag parsing (--version, --help, future --debug flags)
│
└── tests/
    ├── unit/                     # Per-module unit tests, mirrors tests/unit/luna-init/ pattern
    │   ├── drm/
    │   ├── kms/
    │   ├── protocol/
    │   └── ipc/
    ├── fuzz/
    │   └── protocol/               # AFL++ harness for protocol/tlv.c — mirrors tests/fuzz/toml/
    └── integration/
        └── qemu/                   # QEMU-based boot + handshake integration tests
```

## Notes on placement decisions

**Why `render/` (not `gpu/`) is the GPU abstraction layer's top-level name.** `02_rendering_pipeline.md` names this layer `lgp-render` explicitly and is unambiguous that it sits between the compositor core and the GPU backend, with the GPU backend itself isolated underneath it (`render/gpu/vulkan/`, `render/gpu/egl/`). The Stage 2 software path (`render/software/`) is a sibling of the Stage 3 GPU backends under the same abstraction, not a separate top-level directory — this mirrors DL-026's framing that both stages implement the same `lgp-render` API surface.

**Why `scene/zorder.c` is called out as blocked.** Rather than silently picking one of the two conflicting layer-numbering tables found in Phase 2 (`LGP_IMPLEMENTATION_PLAN.md` §2.1-B), the directory plan reserves the file and documents the dependency inline, so the gap is visible to anyone browsing the tree, not just anyone reading the planning documents.

**Why there is no `theme/` directory here.** Theme Engine (`06_theme_engine.md`) and Color Resolver are logically downstream of the Color Semantic Contract resolution (§2.1-A) and are scoped to a later milestone (M6) than this plan covers. When that milestone starts, the natural home is a new top-level `theme/` directory parallel to `scene/`, not nested under `render/`, since the Color Resolver is consulted by `protocol/messages/color_messages.c` as much as by the render pass.

**Why `config/` only holds CLI args here, not theme TOML.** `13_component_ownership.md` assigns ownership of the active-theme TOML file to `luna-settings`, not to the compositor. The compositor only *reads* the resolved theme via the path documented in `06_theme_engine.md` — that read path belongs in the future `theme/` directory, not in `config/`, to keep the ownership boundary in the code structure as clear as it is in the ownership matrix.

**Why `client/` and `scene/` are separate.** `client/` answers "what does this *connection* own" (session, granted capabilities); `scene/` answers "what does the *compositor* render, in what order" (the Z-ordered surface array). A single client can own multiple surfaces; keeping these concerns in separate modules mirrors the separation `03_compositor.md` itself draws between the Surface Manager and the Focus Manager/client policy logic.
