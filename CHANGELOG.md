# Changelog

## v0.2 (2026-06-30)

### Graphical Desktop & Window Manager

- **luna-splash**: Decoupled early boot graphics engine using zero-malloc `/dev/fb0` framebuffer mapping and IPC with `luna-init` for boot progress.
- **lgp-compositor**: DRM/KMS compositor with Z-order, ARGB8888 software alpha blending, and dynamic keyboard event routing.
- **luna-shell**: Native desktop shell and Window Manager providing a wallpaper layer, top status bar, cascading window placement, and hotkeys (`Super+T` to launch terminal, `Alt+Tab` to cycle focus).
- **LunaGUI Toolkit**: Native C17 widget toolkit with recursive widget destruction, layout boxes (VBox/HBox), scroll containers, custom canvas widgets, pointer-button hit testing, and keyboard event routing.
- **Core Applications**: 10 native applications including:
  - `luna-terminal`: ANSI terminal with VT100 sequence parser, scrollback, PTY resize, and clipboard support.
  - `luna-installer`: 10-page graphical OS installation wizard.
  - `luna-settings`, `luna-files`, `luna-calc`, `luna-text`, `luna-about`, `luna-tasks`.
- **PSF Font Engine**: Embedded PSF1 bitmap font loading and pixel-level rendering with bounding box clipping.

### Security & Hardening

- **Capability Enforcement**: Secure capability gate for privileged operations. Strictly enforces `LGP_CAP_CLIPBOARD` for clipboard set/get transactions, and `LGP_CAP_WINDOW_MANAGER` for window placement, state changes, and focus commands.
- **Input Security**: Validated coordinate bounds for `LGP_MSG_WM_SET_SURFACE_POSITION` to prevent out-of-bounds rendering or integer overflow.
- **Compositor Guarding**: Added HELLO-stage handshake verification on the direct pixel path (`LGP_CAP_DIRECT_LGP`).

### Bug Fixes & Refinements

- **fix(gui)**: Resolved incomplete typedef issues by including `widget_private.h` in `application.c` and `window.c`.
- **fix(gui)**: Corrected `lgui_font_draw_text` signature mismatch and integrated canvas clipping stack to prevent buffer overflows.
- **fix(compositor)**: Clamped mouse coordinates against actual DRM display dimensions instead of hardcoded resolution.
- **fix(gui)**: Ensured `lgui_window_update()` handles root widget replacement safely.
- **fix(build)**: Added `keyboard.c`, `wm.c`, `canvas_widget.c`, and `scroll.c` to Makefiles.
- **fix(deploy)**: Updated `build-image.sh` to copy all ten native GUI applications and deploy the font asset.

## v0.1

- Initial bootloader
- luna-init
- Service Manager
- TOML parser
- Dependency graph
- Framebuffer console
