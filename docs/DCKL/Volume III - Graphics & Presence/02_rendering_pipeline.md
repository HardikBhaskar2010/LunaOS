# Mahina — Rendering Pipeline
**Volume III · Chapter 2**
**Classification:** Core Architecture — Graphics Implementation
**Status:** Active · Depends on LGP wire format and GPU backend decisions

---

## Purpose

This document specifies the Mahina rendering pipeline: the path from application-submitted pixel data to displayed pixels on the screen. It defines the stages of compositing, the GPU abstraction layer, the buffer lifecycle, and the frame timing model that produces the Living Interface motion quality.

This document is the authoritative reference for:
- How frames are produced from submitted surface buffers
- GPU abstraction and backend selection
- The DRM/KMS interface from the compositor
- The vsync model and frame timing
- Buffer allocation and DMA-BUF for GPU-accelerated surfaces
- Performance targets and frame budget

---

## Overview

The rendering pipeline begins when an application commits a buffer to the LGP compositor and ends when pixels appear on the physical display. The compositor is the sole owner of this pipeline. Applications participate by submitting buffers at compositor-directed frame times; they do not control any stage of the rendering pipeline beyond their own buffer content.

**Pipeline stages:**

```
Application commits buffer
        │
        ▼
[1] Buffer reception (LGP protocol layer)
        │   Validate, map into compositor address space
        ▼
[2] Damage tracking
        │   Determine which screen regions have changed
        ▼
[3] Scene graph update
        │   Rebuild compositor scene: surface order, transforms, clips
        ▼
[4] GPU render pass
        │   Composite all surface layers into the final framebuffer
        │   Apply semantic color resolution
        │   Apply active animations (from animation engine)
        ▼
[5] KMS scanout
        │   DRM/KMS presents the framebuffer to the display hardware
        ▼
Display — vsync → next frame callback to applications
```

---

## Design Philosophy

### Frame budget drives everything

Living Interface motion quality is not achieved by animating faster — it is achieved by never missing a frame within the animation budget. A 60 Hz display gives 16.67 ms per frame. A 144 Hz display gives 6.94 ms. The rendering pipeline must complete within this window.

**Frame budget allocation (16.67 ms at 60 Hz):**

| Stage | Target time | Notes |
|---|---|---|
| Input processing | < 1 ms | Input events must be processed first for latency |
| Scene graph update | < 2 ms | Damage tracking + scene rebuild |
| GPU render pass | < 8 ms | Actual compositing work |
| KMS submission | < 1 ms | Buffer handoff to display hardware |
| Margin | ~4.67 ms | For vsync timing variance and scheduling jitter |

At 144 Hz the margin shrinks to ~0.7 ms, which requires the GPU render pass to be under 4 ms. This constrains the complexity of concurrent animations that are permissible within the Animation Budget.

### No partial frame presentation

The compositor never presents a partially-composited frame. It either presents a complete frame on a vsync boundary or holds the previous frame. This is the standard "double buffering" compositing model. Screen tearing is not acceptable under any condition.

### Damage tracking reduces GPU work

The compositor tracks which screen regions have changed since the last frame ("damage regions"). Undamaged regions are not re-composited — the compositor can copy them from the previous framebuffer or skip them if the GPU supports it. This is critical for maintaining frame budget on static or mostly-static screens.

---

## Architecture

### GPU Abstraction Layer

The compositor does not call Vulkan or OpenGL directly in its application code. A GPU abstraction layer (`lgp-render`) sits between the compositor and the GPU backend:

```
Compositor core
      │ lgp-render API calls
      ▼
┌─────────────────────────┐
│   lgp-render             │   GPU abstraction layer
│                           │
│   render_target_create()  │
│   render_surface()        │
│   render_composite()      │
│   render_present()        │
└──────────┬───────────────┘
           │
    ┌──────┴──────────────┐
    │                      │
    ▼                      ▼
Vulkan backend        OpenGL/EGL backend
(primary)             (fallback)
    │                      │
    └──────────────────────┘
                │
                ▼
         DRM/KMS device
         /dev/dri/card0
```

The abstraction layer provides a unified API for:
- Creating render targets (framebuffers)
- Compositing surfaces (texture sampling + blending)
- Presenting frames to the display (KMS flip)

This allows the compositor to run on hardware without Vulkan support by falling back to OpenGL/EGL without changing the compositor's compositing logic.

### GPU Backend

Per **DL-026**, the GPU backend is implemented in two stages:
- **Stage 2:** Software renderer (CPU blit to dumb framebuffer). Proves the compositor pipeline, LGP protocol, and rendering architecture before GPU complexity is introduced.
- **Stage 3:** Vulkan as the primary GPU renderer. OpenGL/EGL as a fallback for hardware without Vulkan 1.1+ support.

Vulkan backend provides zero-copy DMA-BUF import and explicit synchronization, necessary for GPU-accelerated CANVAS_SURFACE compositing. OpenGL/EGL provides a fallback path with mature driver support. Both backends will produce the same visual output. The abstraction layer ensures compositor correctness is independent of backend selection.

### DRM/KMS Interface

The compositor interacts with display hardware through the Linux kernel's DRM/KMS subsystem:

**DRM (Direct Rendering Manager):**
- The compositor opens `/dev/dri/card0` (or the appropriate DRM device)
- DRM manages GPU command buffers and rendering contexts
- The compositor is the exclusive owner of the DRM device — no other process may open it

**KMS (Kernel Mode Setting):**
- KMS manages display output configuration: resolution, refresh rate, CRTC/connector assignment
- The compositor calls KMS to configure outputs at startup and when display configuration changes
- Frame presentation is done via **DRM page flip**: the compositor submits a framebuffer to KMS and KMS presents it at the next vsync

**DRM device discovery:**

```c
// Compositor startup — DRM device selection
// Mahina v1 assumes a single GPU. Multi-GPU is a post-v1 concern.
int drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
if (drm_fd < 0) {
    // Try /dev/dri/card1 (secondary GPU)
    drm_fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
}
// Enumerate KMS connectors and select active display output
```

```
TODO:
Decision not yet finalized.
Reason: DRM device selection for multi-GPU systems (integrated + discrete)
has not been specified. v1 assumes a single GPU and opens /dev/dri/card0.
Multi-GPU support is a post-v1 architectural item.
```

### Buffer Types

The compositor handles three types of surface buffers:

**Type 1 — Shared Memory (shm) buffers:**
- Used by: LunaGUI applications (CPU rendering)
- Mechanism: Application allocates via `shm_open()`, passes FD to compositor via `SCM_RIGHTS`
- GPU upload: Compositor uploads shm buffer to GPU texture before render pass
- Cost: One CPU→GPU copy per frame for this surface

**Type 2 — DMA-BUF buffers:**
- Used by: Direct-LGP CANVAS_SURFACE with GPU rendering (games, video decoders, GPU compute)
- Mechanism: Application creates DMA-BUF via `/dev/dma_heap` or GPU driver, passes FD to compositor
- GPU import: Compositor imports DMA-BUF as a Vulkan external image (or EGL image) — zero copy
- Cost: Zero CPU→GPU copy — the buffer is already in GPU memory

**Type 3 — Compositor-owned buffers:**
- Used by: LUNA_ISLAND, SYSTEM_CHROME, SHELL_SURFACE surfaces
- Mechanism: Compositor allocates and renders these surfaces itself (they are compositor-native)
- Cost: No buffer transfer — compositor renders directly

### Compositing Algorithm

For each frame, the compositor composites surfaces in Z-order from bottom to top:

```
Frame render sequence:

1. Clear framebuffer (or carry over previous frame for undamaged regions)

2. For each surface in Z-order (bottom to top):
   a. If surface is damaged:
      - For shm surfaces: upload CPU buffer to GPU texture
      - For DMA-BUF surfaces: ensure GPU sync fence has signaled
      - Composite surface texture onto framebuffer at surface geometry
      - Apply active animation transforms (from animation engine)
      - Resolve semantic colors via Color Resolver
   b. If surface is not damaged:
      - Skip (damage tracking optimization)

3. Apply global color effects (if theme has global transforms)

4. Submit completed framebuffer to KMS via page flip
```

### Vsync and Frame Timing

The compositor drives frame timing from the DRM vblank interrupt:

```
DRM vblank interrupt fires (display hardware)
         │
         ▼
Compositor vblank handler:
  1. Record vblank timestamp
  2. Send LGP_FRAME_CALLBACK to all surfaces that requested one
  3. Applications render their next frame
  4. Applications commit buffers
         │
         ▼  (one display period later)
Compositor frame assembly:
  1. Process all committed buffers from this cycle
  2. Run render pass (damage tracking → GPU composite → KMS flip)
         │
         ▼  (next vblank)
Display presents new frame
```

This is a **double-buffered** presentation model. While the GPU renders frame N+1, the display is scanning out frame N. When the GPU finishes, it signals KMS to flip at the next vsync.

**Adaptive sync (variable refresh rate):**

```
TODO:
Decision not yet finalized.
Reason: Adaptive sync (NVIDIA G-Sync / AMD FreeSync) support has not been decided.
VRR (variable refresh rate) via the DRM VRR extension would allow the compositor
to present frames slightly early or late without tearing.
This is particularly beneficial for direct-LGP game clients.
Must be a Decision Log entry. Not a v1 priority.
```

---

## Technical Details

### Frame Budget Monitoring

The compositor tracks per-frame timing to detect budget violations:

```c
// Per-frame timing record (compositor internal)
typedef struct {
    uint64_t vblank_ts;         // Vblank interrupt timestamp (ns)
    uint64_t scene_update_ts;   // Scene graph update complete (ns)
    uint64_t gpu_submit_ts;     // GPU render pass submitted (ns)
    uint64_t gpu_complete_ts;   // GPU render pass complete (ns)
    uint64_t kms_flip_ts;       // KMS page flip submitted (ns)
    uint32_t frame_number;      // Monotonic frame counter
    bool     budget_exceeded;   // True if any stage ran over target
} lgp_frame_timing_t;
```

When `budget_exceeded` is true, the compositor logs the violation to `/var/log/luna-init/runtime.log` at WARN level. Repeated budget violations trigger an animation engine downgrade (reduce concurrent animation complexity).

### Animation Engine Integration

The compositor's rendering pipeline integrates directly with the animation engine (specified in `05_animation_engine.md`). At each frame render:

1. Animation engine provides the current transform state for all active animations
2. Renderer applies transforms (translation, scale, opacity, blur radius) to each surface
3. Animation engine advances state by one frame
4. Animation engine checks if any animation has exceeded its budget ceiling; if so, it snaps to the final state

The animation engine does not have its own timer — it advances in lockstep with the rendering pipeline's frame clock.

### Compositor Render Target

The compositor renders to a GPU-allocated framebuffer:

```
TODO:
Decision not yet finalized.
Reason: Framebuffer format has not been decided.
Candidates:
  - ARGB8888 (32-bit per pixel, 8 bits per channel) — standard, wide support
  - RGBA1010102 (10-bit color) — better HDR range
  - RGBA16F (16-bit float, HDR) — future, requires HDR display support
v1 target: ARGB8888. HDR support is post-v1.
```

### Bootloader-to-Compositor Transition

At Stage 5 of boot, the framebuffer boot splash (rendered by `luna-init`) must transition to the compositor:

Per **DL-043**, Mahina accepts a brief visual transition (single black frame, ~16ms at 60Hz) between the luna-init framebuffer boot splash and the LGP compositor's first frame. No architectural complexity is introduced to eliminate this cut. The boot splash renderer (luna-init) stops rendering before the compositor takes over, and the lgp-compositor's first frame is its own rendered output.

---

## Performance Targets

| Metric | v1 Target | Notes |
|---|---|---|
| Frame render time (60 Hz display) | < 12 ms | Leaves 4.67 ms margin |
| Frame render time (144 Hz display) | < 4 ms | Tight budget — simple animations only |
| Input-to-display latency | < 33 ms (2 frames at 60 Hz) | From input event to pixel on screen |
| Boot splash to compositor handoff | < 100 ms | Perceptually instantaneous |
| Damage tracking coverage | > 80% frames have partial damage only | Full redraw is expensive |
| GPU memory usage (compositor) | < 256 MB | Framebuffers + surface textures |

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Adaptive sync (VRR) | v1.5 | Benefits game CANVAS_SURFACE clients |
| HDR output (10-bit / 16-bit) | v2 | Requires HDR-capable display + GPU |
| Multi-GPU compositor | v2 | Route application surfaces to different GPUs |
| Hardware overlay planes | v1.5 | Bypass GPU composite for static surfaces — reduces power |
| CANVAS_SURFACE rotation/transform | v1.5 | For games needing rotated output |

---

## Open Questions

1. **Framebuffer format.** ARGB8888 for v1. HDR format for v2. Must be documented in a DL entry.

2. **Adaptive sync (VRR).** Not a v1 priority. Decision Log entry when v1.5 work begins.

3. **Multi-GPU.** Single GPU for v1. Decision Log entry when multi-GPU work begins.

4. **GPU memory limit.** The 256 MB compositor GPU memory target is an estimate. Depends on number of simultaneous surfaces and resolution. Must be validated on reference hardware.

---

## AI Context

An AI agent implementing the Mahina rendering pipeline must understand:

- The compositor renders all surfaces. Application code never calls GPU APIs directly except through a CANVAS_SURFACE (and even then, the compositor composites the result).
- The frame timing model is compositor-driven: wait for `LGP_FRAME_CALLBACK`, render, commit. Never render in a tight loop without a callback.
- The GPU abstraction layer (`lgp-render`) separates Vulkan/OpenGL specifics from the compositor's compositing logic. If the GPU backend changes, only `lgp-render` changes — not the compositor's scene management code.
- DRM is the kernel interface. The compositor opens `/dev/dri/card0`. No other Mahina process opens the DRM device.
- Shm buffers (CPU rendering) require a CPU→GPU upload each frame. DMA-BUF (GPU rendering) is zero-copy. The compositor handles both transparently.
- Budget violations are logged, not silently dropped. If a frame exceeds budget, it is logged at WARN. Repeated violations should trigger complexity reduction.
- The animation engine runs in the compositor's render thread, not in a separate thread. It advances one frame per vsync.
- All output configuration (resolution, refresh rate, display detection) goes through KMS. The compositor owns KMS. Applications learn about display outputs through LGP events, not by accessing KMS directly.

---

*Document: `Volume III / 02_rendering_pipeline.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: 01_lgp.md, core_laws.md (Law III — Animation Budget), living_interface_design.md, decision_log.md (DL-004R), linux_architecture.md*
*Informs: 03_compositor.md, 05_animation_engine.md*
