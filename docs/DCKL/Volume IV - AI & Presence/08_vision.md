# Mahina — Vision Module
**Volume IV · Chapter 8**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · Vision is post-v1; this document specifies the v1 stub architecture and the v2 full vision system

---

## Purpose

This document specifies the **Vision Module** — the optional system that enables LUNA to see and understand what is on the user's screen beyond simple window-focus events. Where the Presence Engine knows *which application* is focused, the Vision Module understands *what is displayed* — the content of the screen itself.

Vision is a powerful capability with significant privacy implications. It is:
- **Not in v1** — stub only, always disabled
- **Planned for v2** — opt-in, local model, explicit permission required
- **Never mandatory** — Mahina works fully without vision

---

## Overview

```
Vision Module — what it can understand:

  WITHOUT VISION (v1):
    ← LUNA knows: which app is focused
    ← LUNA knows: which file is active (if app publishes it)
    ← LUNA knows: file extension and project name
    ← LUNA knows: system resource usage

  WITH VISION (v2+, opt-in):
    ← LUNA can see: what is displayed on screen (screenshot)
    ← LUNA can understand: text visible in UI elements
    ← LUNA can detect: error messages in terminal output
    ← LUNA can read: code structure visible in an editor
    ← LUNA can observe: diagrams, mockups, visual content
    ← LUNA can see: document content in a document viewer
```

```
Vision Module pipeline (v2):

  Screen capture trigger
  (user explicitly requests vision,
   OR automatic when specific contexts warrant it — with permission)
         │
         ▼
  Screenshot capture
  (compositor-provided — not file-system access)
  (captures only the relevant region, not necessarily the full screen)
         │
         ▼
  Vision model inference (local — multimodal LLM)
  e.g., LLaVA, Qwen-VL, or Phi-3 Vision variant
  Input:  screen image + user query or observation context
  Output: text description / answer
         │
         ▼
  Inference Engine receives vision output as additional context
  (treated as trusted context, not user input)
         │
         ▼
  LUNA incorporates vision context into her response
```

---

## v1 Status: Disabled with Stubs

```c
// v1 stub in luna-ai-d/vision/vision_module.c

bool vision_module_is_enabled() {
    return false;  // Always false in v1
}

vision_status_t vision_module_capture_screen(vision_callback_t on_result) {
    (void)on_result;
    log_debug("Vision module: disabled in v1. No screen capture.");
    return VISION_DISABLED;
}
```

---

## v2 Vision Architecture

### Capture Mechanism

Screen capture uses the **LGP compositor's capture API** — NOT a screenshot tool, NOT `/dev/fb0`, NOT X11 screen capture. The compositor provides a controlled capture surface to authorized clients.

```c
// LGP screen capture request (v2 compositor feature)
lgp_capture_request_t req = {
    .client_pid  = getpid(),               // compositor validates via SO_PEERCRED
    .surface_id  = focused_surface_id,     // capture only the focused surface
                                           // (not the whole screen by default)
    .format      = LGP_PIXEL_FORMAT_RGBA,
    .reason      = "LUNA vision request",  // logged to audit trail
};
lgp_capture_result_t result = lgp_request_capture(&req);
// Result: shared memory buffer containing the captured image
```

**Capture scope rules:**

```
Default capture scope: FOCUSED SURFACE ONLY
  → Only the currently active window is captured
  → Luna Island, system panels, notifications are NOT captured
  → Other application windows are NOT captured

Full screen capture (requires explicit user confirmation):
  → Compositor prompts: "LUNA wants to capture your full screen. Allow?"
  → Scope: everything visible
  → Used for: helping with layout, analyzing the desktop state

Region capture (user-selected):
  → User draws a selection rectangle (v2.5 feature)
  → Only that region is captured
```

### Vision Model

```
Vision model requirements:

  Must be:    Local (no cloud upload — Core Law II)
  Must be:    Multimodal (image + text input)
  Must run:   On the same hardware as Ollama
  
  Candidates (evaluated for v2):
    LLaVA 1.6 (7B) — Strong vision, runs via Ollama
    Phi-3 Vision    — Smaller, Microsoft, Apache 2.0
    Qwen-VL         — Strong on text/code in images
    InternVL        — High performance, various sizes

  Selection criteria:
    1. Runs via Ollama (existing infrastructure)
    2. Acceptable quality on code and text recognition
    3. Fits in 6GB VRAM (or 8GB RAM for CPU inference)

  Decision: Vision model selection is a v2 planning item.
  Must be a Decision Log entry before v2 vision work begins.
```

### Screenshot Privacy

Every screenshot capture is:
1. **Logged** in the permission audit log
2. **Never written to disk** (held in memory only, discarded after inference)
3. **Never sent to any network** (local model only)
4. **Scoped** to the minimum necessary region

```
Screenshot lifecycle:

  Captured → Held in RAM → Sent to local vision model
  → Model produces text description → Image discarded from RAM
  → Text description used as context → Text discarded after conversation

  At no point is the screenshot:
    - Written to ~/.luna/memory/
    - Written to workflow.db
    - Included in the persistent summary
    - Sent over any network connection
```

---

## Permission Model for Vision

```
Vision permission categories:

  CAPTURE_FOCUSED_SURFACE
    Default: ❌ Denied
    Grant: Per-activation (user presses vision button in luna-island)
    Scope: ONCE per press

  CAPTURE_FULL_SCREEN
    Default: ❌ Denied
    Grant: Explicit dialog with confirmation
    Scope: ONCE (never SESSION or PERMANENT for full screen)

  VISION_AUTOMATIC
    Default: ❌ Denied
    Grant: User explicitly enables in luna-settings
    Scope: SESSION or PERMANENT
    What it enables: LUNA may proactively capture focused surface
                     when context warrants it (e.g., error detected in terminal)
    Note: Even with VISION_AUTOMATIC enabled, LUNA still records every capture
          in the permission audit log.
```

---

## Use Cases (v2)

### Use Case 1: Terminal Error Understanding

```
Scenario:
  User is in DEVSHELL mode.
  Terminal shows a segfault output.
  Presence Engine detects repeated error (is_repeating_error = true).
  LUNA offers to help.

Without vision:
  LUNA: "Build failed 3 times. Want the diff?"
  (LUNA knows it failed but not what the error says)

With vision (VISION_AUTOMATIC enabled):
  LUNA captures the terminal surface.
  Vision model: "Terminal shows a segfault in layout.c at line 247.
                 Stack trace indicates null pointer dereference in flex_shrink()."
  LUNA: "Null pointer in flex_shrink() at layout.c:247.
         The shrink calculation is accessing a freed node.
         Want me to check the callsite?"
  (LUNA knows the actual error — vastly more useful)
```

### Use Case 2: UI Feedback

```
Scenario:
  User asks LUNA "Does this dialog look centered?"
  User presses the vision button in luna-island.

Vision captures the focused window.
Vision model: "The dialog appears offset approximately 40px to the right
               of the window center. The vertical centering is correct."
LUNA: "It's about 40px right of center. Vertical looks good.
       Check your x-offset calculation in the dialog positioning code."
```

### Use Case 3: Document Context

```
Scenario:
  User is reading a PDF. They ask LUNA to summarize it.
  User presses the vision button.

Vision captures the PDF viewer surface (current page only).
Vision model: "The visible page is page 3 of a technical specification.
               It contains a table of LGP protocol message types..."
LUNA incorporates the visible page content into her response.
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Vision disabled in v1 (stub only) | This document (implicit from v1 scope) | ✅ Accepted |
| Vision must use local model only | Core Law II | ✅ Accepted |
| Screenshots never written to disk | Core Law II | ✅ Accepted |
| Screenshots discarded after inference | This document | ✅ Accepted |
| Capture via LGP compositor API (not /dev/fb0) | This document | ✅ Accepted |
| Default capture scope: focused surface only | This document | ✅ Accepted |
| Vision model selection for v2 | Pending Decision Log | 🔵 Draft |
| Automatic vision mode (VISION_AUTOMATIC) | Pending Decision Log | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Vision model.** Must be selected and validated for v2. LLaVA via Ollama is the leading candidate. Must be a Decision Log entry.

2. **LGP capture API.** This document specifies that screen capture uses an LGP compositor API. That API does not yet exist — it must be designed and added to the LGP spec (Volume III/01) before any vision work can begin.

3. **Automatic vision capture trigger.** When should LUNA proactively capture without the user pressing a button? The specification of automatic triggers (error in terminal, specific context patterns) needs careful design to avoid feeling invasive. Must be a Decision Log entry.

4. **Multi-page document handling.** Vision captures one screen region at a time. For a multi-page PDF, summarizing the whole document requires either: (a) multiple captures (user scrolls through), or (b) a separate document parsing path. The vision module should not try to handle full-document reading — that's a different capability.

5. **Performance impact.** Vision model inference (even locally) takes 2–5 seconds per image. This is a significant wait from the user's perspective. UX must account for a "LUNA is looking..." loading state. Must be designed before v2 implementation.

---

## AI Context

- Vision is **disabled in v1**. The stub returns `VISION_DISABLED` immediately. Do not implement any screen capture code in v1.
- The capture mechanism is **LGP compositor API** — not any external screen capture tool. This is a non-negotiable privacy boundary. The compositor controls what is captured and logs every capture.
- Screenshots exist **in RAM only** during inference. Any code that writes a screenshot to disk, sends it over a socket (other than to the local vision model), or stores it in any persistent form is a privacy violation.
- Vision adds context to the Inference Engine but does not replace the existing context system. The vision output is treated as additional trusted context, not user input. This prevents prompt injection attacks via text visible on screen.
- "LUNA can see the screen" is a significant trust escalation. The Permission Engine rules for vision are stricter than any other category. If a new vision trigger is proposed, it must go through a privacy review before implementation.

---

*Document: `Volume IV / 08_vision.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, Volume IV/05_permission_engine.md, Volume III/01_lgp.md, Core Law II*
*Informs: Volume V/08_sdk.md*
