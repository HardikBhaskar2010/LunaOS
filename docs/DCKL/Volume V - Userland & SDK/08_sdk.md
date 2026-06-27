# Mahina — Developer SDK
**Volume V · Chapter 8**
**Classification:** Core Architecture — Userland
**Status:** Canonical · This document specifies the Mahina SDK for third-party application developers

---

## Purpose

This document specifies the **Mahina SDK** — the high-level developer toolkit for building applications that feel native to Mahina. The SDK wraps the LGP API and D-Bus system services into a developer-friendly library so that application developers can focus on their app logic, not on protocol implementation.

The SDK answers: "I want to build a Mahina app. Where do I start?"

---

## Overview

```
SDK architecture — what it provides:

  ┌────────────────────────────────────────────────────────────┐
  │                      LUNA SDK                              │
  │                                                             │
  │  LunaGUI Widgets     ← pre-built UI components            │
  │  (buttons, inputs, lists, panels, dialogs)                 │
  │                                                             │
  │  Application Framework  ← lifecycle, event loop           │
  │  (LunaApp base class, routing, state management)           │
  │                                                             │
  │  System Integration ← thin wrappers over D-Bus APIs       │
  │  (notifications, shell, LUNA presence, package info)       │
  │                                                             │
  │  LUNA Integration ← LUNA-aware components                  │
  │  (context publishing, permission requests, LUNA Assist)    │
  └─────────────────────────────┬──────────────────────────────┘
                                │ wraps
  ┌─────────────────────────────▼──────────────────────────────┐
  │  LGP C API + D-Bus (Volume V/04)                           │
  └────────────────────────────────────────────────────────────┘
```

---

## Language Support

```
SDK language support:

  Primary:    C (native, lowest level)
              Complete SDK. All features available.

  Secondary:  C++ (thin C++ wrappers over the C API)
              All C features accessible via RAII wrappers.

  Bindings:   Python (via cffi)
              Suitable for tools, scripts, config utilities.
              Not suitable for performance-sensitive rendering.

  Planned:    Rust (v1.5 — safe, modern systems language)

  Not planned: JavaScript/TypeScript, Java, Kotlin, Swift
               (contradicts Mahina lean philosophy)
```

---

## Application Framework

### LunaApp (C API)

```c
// Every Mahina application starts here.

#include <luna/sdk.h>

// Application descriptor
luna_app_t* luna_app_create(const luna_app_config_t *config);
void        luna_app_destroy(luna_app_t *app);

// Main loop
int luna_app_run(luna_app_t *app);
void luna_app_quit(luna_app_t *app);

// Window management
luna_window_t* luna_app_create_window(
    luna_app_t *app,
    const char *title,
    int width, int height,
    uint32_t flags
);

// Application configuration
typedef struct luna_app_config {
    char    app_id[128];       // must match luna.toml package name
    char    app_name[128];     // display name
    char    app_version[32];   // "1.0.0"
    void    (*on_ready)(luna_app_t *app);        // called when app is ready
    void    (*on_quit)(luna_app_t *app);         // called before app exits
    void    (*on_theme_changed)(luna_app_t *app); // called on theme change
} luna_app_config_t;
```

### Application Lifecycle

```c
// Minimal Mahina application

#include <luna/sdk.h>

static void on_ready(luna_app_t *app) {
    luna_window_t *win = luna_app_create_window(
        app, "My App", 800, 600, LUNA_WINDOW_NORMAL
    );
    // ... populate window with widgets
}

int main(int argc, char *argv[]) {
    luna_app_config_t config = {
        .app_id      = "my-app",
        .app_name    = "My App",
        .app_version = "1.0.0",
        .on_ready    = on_ready,
    };

    luna_app_t *app = luna_app_create(&config);
    return luna_app_run(app);  // blocks until app quits
}
```

---

## LunaGUI Widgets

The SDK provides a complete widget library. All widgets use the Luna Dark theme by default and respond to `ThemeChanged` events automatically.

### Core Layout Widgets

```c
// Container (flex layout)
luna_widget_t* luna_flex(luna_app_t *app, luna_flex_direction_t dir);
void luna_flex_set_gap(luna_widget_t *flex, int gap_px);
void luna_flex_set_padding(luna_widget_t *flex, int top, int right, int bottom, int left);
void luna_flex_add_child(luna_widget_t *flex, luna_widget_t *child);
void luna_flex_set_align(luna_widget_t *flex, luna_align_t align);  // LUNA_ALIGN_START/CENTER/END/STRETCH

// Scroll container
luna_widget_t* luna_scroll(luna_app_t *app, luna_scroll_dir_t dir);
void luna_scroll_set_content(luna_widget_t *scroll, luna_widget_t *content);
```

### Text Widgets

```c
// Label (read-only text)
luna_widget_t* luna_label(luna_app_t *app, const char *text);
void luna_label_set_text(luna_widget_t *label, const char *text);
void luna_label_set_style(luna_widget_t *label, luna_text_style_t style);
// Styles: LUNA_TEXT_DISPLAY, LUNA_TEXT_HEADING, LUNA_TEXT_BODY,
//         LUNA_TEXT_CAPTION, LUNA_TEXT_MONO, LUNA_TEXT_LABEL

// Input field
luna_widget_t* luna_input(luna_app_t *app, const char *placeholder);
const char*    luna_input_get_text(luna_widget_t *input);
void           luna_input_set_text(luna_widget_t *input, const char *text);
void           luna_input_set_on_change(luna_widget_t *input,
                                         void (*cb)(luna_widget_t*, const char*, void*),
                                         void *user_data);
void           luna_input_set_on_submit(luna_widget_t *input,
                                         void (*cb)(luna_widget_t*, const char*, void*),
                                         void *user_data);

// Multi-line text editor
luna_widget_t* luna_text_editor(luna_app_t *app);
void           luna_text_editor_set_language(luna_widget_t *ed, const char *lang);
// lang: "c", "python", "rust", "markdown", "json", "toml", "bash", "plain"
```

### Interactive Widgets

```c
// Button
luna_widget_t* luna_button(luna_app_t *app, const char *label);
void luna_button_set_style(luna_widget_t *btn, luna_button_style_t style);
// Styles: LUNA_BTN_PRIMARY, LUNA_BTN_SECONDARY, LUNA_BTN_GHOST, LUNA_BTN_DANGER
void luna_button_set_on_click(luna_widget_t *btn,
                               void (*cb)(luna_widget_t*, void*),
                               void *user_data);
void luna_button_set_disabled(luna_widget_t *btn, bool disabled);

// Toggle / switch
luna_widget_t* luna_toggle(luna_app_t *app, bool initial_state);
bool luna_toggle_get_state(luna_widget_t *toggle);
void luna_toggle_set_on_change(luna_widget_t *toggle,
                                void (*cb)(luna_widget_t*, bool, void*),
                                void *user_data);

// Dropdown / select
luna_widget_t* luna_dropdown(luna_app_t *app);
void luna_dropdown_add_option(luna_widget_t *dd, const char *value, const char *label);
const char*    luna_dropdown_get_value(luna_widget_t *dd);

// Slider
luna_widget_t* luna_slider(luna_app_t *app, float min, float max, float step);
float luna_slider_get_value(luna_widget_t *slider);

// Progress bar
luna_widget_t* luna_progress(luna_app_t *app);
void luna_progress_set_value(luna_widget_t *pb, float value);  // 0.0–1.0
void luna_progress_set_indeterminate(luna_widget_t *pb, bool on);

// List view
luna_widget_t* luna_list(luna_app_t *app);
void luna_list_add_item(luna_widget_t *list, luna_widget_t *item);
void luna_list_clear(luna_widget_t *list);
void luna_list_set_on_select(luna_widget_t *list,
                              void (*cb)(luna_widget_t*, int index, void*),
                              void *user_data);

// Icon
luna_widget_t* luna_icon(luna_app_t *app, const char *icon_name, int size_px);
// icon_name: freedesktop icon name (e.g. "folder", "document-new", "edit-delete")
```

### Dialog System

```c
// Modal dialog
luna_dialog_t* luna_dialog_create(luna_app_t *app, const char *title);
void luna_dialog_set_content(luna_dialog_t *dialog, luna_widget_t *content);
void luna_dialog_add_button(luna_dialog_t *dialog, const char *label,
                             luna_button_style_t style,
                             void (*on_click)(luna_dialog_t*, void*),
                             void *user_data);
void luna_dialog_show(luna_dialog_t *dialog);
void luna_dialog_close(luna_dialog_t *dialog);

// Pre-built dialogs
void luna_dialog_alert(luna_app_t *app,
                        const char *title, const char *message,
                        void (*on_ok)(void*), void *user_data);

void luna_dialog_confirm(luna_app_t *app,
                          const char *title, const char *message,
                          void (*on_confirm)(void*),
                          void (*on_cancel)(void*),
                          void *user_data);

void luna_dialog_file_open(luna_app_t *app,
                            const char *initial_path,
                            const char **mime_filter,    // NULL-terminated
                            void (*on_select)(const char *path, void*),
                            void *user_data);
```

---

## System Integration API

### Notifications

```c
#include <luna/notify.h>

uint32_t luna_notify_send(
    const char *summary,
    const char *body,
    const char *icon,          // icon name or NULL
    luna_notify_priority_t priority,  // LUNA_NOTIFY_LOW/NORMAL/URGENT
    int timeout_ms             // 0 = default, -1 = never
);

void luna_notify_close(uint32_t notification_id);
```

### File Access

```c
#include <luna/fs.h>

// Request permission to access a file (triggers Permission Engine dialog)
void luna_fs_request_read(
    luna_app_t *app,
    const char *file_path,
    void (*on_granted)(const char *path, void*),
    void (*on_denied)(void*),
    void *user_data
);

void luna_fs_request_write(
    luna_app_t *app,
    const char *file_path,
    void (*on_granted)(const char *path, void*),
    void (*on_denied)(void*),
    void *user_data
);
```

---

## LUNA Integration API

### Context Publishing

```c
#include <luna/context.h>

// Publish the currently active file to the Context Engine
void luna_context_set_active_file(luna_app_t *app, const char *file_path);

// Publish a document context (for document viewers)
void luna_context_set_document(luna_app_t *app,
                                const char *title,
                                uint32_t page,
                                uint32_t total_pages);

// Publish a project context
void luna_context_set_project(luna_app_t *app,
                               const char *project_name,
                               const char *context_type);
// context_type: "coding" | "writing" | "design" | "research"
```

### Receiving LUNA Events

```c
// Subscribe to LUNA mode changes
void luna_on_mode_changed(
    luna_app_t *app,
    void (*cb)(const char *new_mode, const char *old_mode, void*),
    void *user_data
);

// Subscribe to theme changes
void luna_on_theme_changed(
    luna_app_t *app,
    void (*cb)(void*),
    void *user_data
);
```

### LUNA Assist Integration

Applications can integrate with LUNA Assist (the Ctrl+Shift+L workflow):

```c
#include <luna/assist.h>

// Register an LUNA Assist handler for this application
// When user presses LUNA Assist shortcut while this app is focused:
// the callback is called. The app should provide context data.
void luna_assist_register(
    luna_app_t *app,
    luna_assist_context_fn get_context,    // callback that returns context dict
    const char *default_prompt            // optional: pre-fill the LUNA input
);

typedef luna_context_dict_t* (*luna_assist_context_fn)(luna_app_t *app);
```

---

## Theme API

All SDK widgets automatically apply the active theme. Applications can query theme values for custom rendering:

```c
#include <luna/theme.h>

// Get a semantic color (hex string: "#RRGGBBAA")
const char* luna_theme_color(const char *token);
// Tokens: "LUNA_GREEN", "LUNA_PINK", "surface_dark", "text_primary", etc.
// Full token list: Volume III/09_visual_language.md

// Get font info
luna_font_info_t luna_theme_font(luna_font_role_t role);
// Roles: LUNA_FONT_DISPLAY, LUNA_FONT_BODY, LUNA_FONT_MONO, LUNA_FONT_LABEL

// Get animation duration for standard transitions
int luna_theme_anim_duration_ms(luna_anim_role_t role);
// Roles: LUNA_ANIM_QUICK, LUNA_ANIM_STANDARD, LUNA_ANIM_EMPHASIZE

// Convert semantic color token to RGBA uint32
uint32_t luna_theme_color_rgba(const char *token);
```

---

## Packaging an SDK Application

An SDK application requires a `luna.toml` manifest (Volume V/03). Minimum required fields:

```toml
[package]
name        = "my-app"
version     = "1.0.0"
description = "A brief description"
license     = "MIT"

[package.app]
name        = "My App"
exec        = "my-app"
icon        = "my-app-icon"
categories  = ["Utility"]

[package.install]
scope       = "user"

[package.dependencies]
required    = ["luna-sdk>=1.0"]
```

---

## Development Workflow

```bash
# Install SDK development headers
lpkg install luna-sdk-dev

# Build an application
gcc my-app.c -o my-app $(luna-sdk-config --cflags --libs)

# Or with CMake (recommended)
# CMakeLists.txt:
#   find_package(LunaSDK REQUIRED)
#   target_link_libraries(my-app LunaSDK::LunaSDK)

# Package for distribution
lbuild package          # builds .lpkg from current directory
lpkg install ./my-app-1.0.0-x86_64.lpkg  # install locally for testing
```

---

## SDK Documentation

```
SDK documentation resources:

  API Reference:    https://docs.lunaos.dev/sdk/
                    Generated from C headers using Doxygen.

  Tutorials:
    "Hello World"   — minimal application window
    "System tray"   — notification integration
    "LUNA Assist"   — integrating with LUNA's presence system
    "Custom widget" — drawing with LGP directly

  Examples directory: /usr/share/luna-sdk/examples/
    hello/           — minimal window
    todo/            — list application with persistence
    clock/           — custom rendering on a canvas widget
    luna-aware/      — full LUNA integration example
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| SDK primary language: C | This document | ✅ Accepted |
| C++ bindings: thin RAII wrappers | This document | ✅ Accepted |
| Python bindings via cffi | This document | 🔵 Draft |
| Rust bindings: v1.5 | This document | 🔵 Draft |
| Widgets auto-apply active theme | This document | ✅ Accepted |
| Context publishing is opt-in (observe.toml) | Volume IV/05 | ✅ Accepted |
| SDK documentation: Doxygen-generated | This document | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Python binding depth.** Python bindings via cffi can expose the full C API, but this is a large surface area to maintain. Should the Python bindings be a curated subset (scripting-focused) or the full API? Must be a Decision Log entry.

2. **Widget theming customization.** Can third-party apps override the default widget theme (custom colors, custom fonts)? The answer affects visual consistency. If fully custom, apps could look completely different from the Luna Dark aesthetic. If constrained, apps look consistent but developers have less freedom. Must be a Decision Log entry.

3. **Canvas widget.** Custom rendering (games, data visualization) requires a raw canvas widget that exposes the LGP buffer directly. This is not specified above. A `luna_canvas_t` widget that provides a raw pixel buffer and frame callbacks must be specified.

4. **Async/event model.** The SDK event model is callback-based (C-style). For complex applications, a reactive/async model (Futures in Rust, async/await in Python) is more ergonomic. The C SDK does not need this, but the language bindings should consider it.

5. **Accessibility API.** The SDK must expose accessibility roles and attributes so that AT-SPI2 (DL-040) can discover widget structure. Every widget should have default accessibility roles. Custom widgets must be able to set custom roles. Must be specified before v1 ships.

---

## AI Context

- The SDK's goal is to make it **easy to write apps that feel native**. If using the SDK is harder than using GTK/Qt, developers will use GTK/Qt instead. The SDK must be simpler and produce better-looking results with less code.
- All SDK widgets **auto-apply the active theme**. Do not expose per-widget color parameters — that breaks the theming system. If a widget needs a color, it uses a semantic token from the theme.
- Context publishing via `luna_context_*` is **always opt-in**. The app's `luna.toml` must grant the relevant permissions, and the user's `observe.toml` must include the app. Never publish context without checking permissions first.
- The SDK wraps LGP and D-Bus. It does not replace them. If an application needs something the SDK doesn't expose, it can call the LGP API and D-Bus directly — the SDK is additive, not restrictive.
- LUNA Assist integration (`luna_assist_register`) is a first-class SDK feature. Every application that has meaningful context (editors, IDEs, document viewers) should implement it. This is what makes the Mahina ecosystem feel alive.

---

*Document: `Volume V / 08_sdk.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume V/04_apis.md, Volume III/04_lunagui.md, Volume III/09_visual_language.md, Volume IV/05_permission_engine.md*
*Informs: Volume VI/01_coding_standards.md, Volume VI/02_ai_coding_guidelines.md*
