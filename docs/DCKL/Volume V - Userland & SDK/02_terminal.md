# LunaOS — Terminal
**Volume V · Chapter 2**
**Classification:** Core Architecture — Userland
**Status:** Canonical · Specifies luna-terminal, the LunaOS native terminal emulator

---

## Purpose

This document specifies **luna-terminal** — the native terminal emulator of LunaOS. The terminal is one of the most used applications on any developer-oriented OS, and LunaOS ships its own to ensure it integrates deeply with LUNA's AI presence.

luna-terminal is not just a terminal. It is the primary interface between the user's command-line work and LUNA's context awareness. Every command run in luna-terminal is a signal the Presence Engine can observe. Every build failure is a LUNA observation opportunity.

This document specifies:
- Terminal rendering (VTE-based or native)
- Integration with luna-ai-d for DEVSHELL mode context
- The command observation protocol
- Configuration and theming
- Multiplexer support

---

## Overview

```
luna-terminal — relationship to LUNA:

  User types command
       │
       ▼
  luna-terminal executes (via PTY)
  └── Publishes to D-Bus: CommandExecuted(cmd, cwd)
       │
       ▼
  Context Engine receives CommandExecuted signal
  └── Updates context_snapshot (classified_context = CODING)
       │
       ▼
  Build failure? (exit code ≠ 0)
  └── luna-terminal publishes: CommandFailed(cmd, exit_code, stderr_snippet)
       │
       ▼
  Presence Engine: is_repeating_error?
  └── Personality Engine: offer help via luna-island
```

luna-terminal is a **standard LunaGUI application** — it creates an `APPLICATION_WINDOW` surface and renders using the LunaGUI toolkit. It is not a privileged process.

---

## Architecture

### Rendering Engine Choice

```
Terminal rendering approach — v1:

  Option A: VTE (GNOME Virtual Terminal Emulator library)
    Pros: Mature, full Unicode/escape-sequence support, well-tested
    Cons: GTK dependency (heavy, contradicts LunaOS lean philosophy)
    Verdict: Rejected

  Option B: Custom terminal emulator (PTY + escape sequence parser)
    Pros: No external dependencies, full control, Luna-themed
    Cons: Significant implementation complexity (months of work)
    Verdict: v2

  Option C: Embed a known lightweight terminal library
    Options: libvte (VTE without GTK), st (simple terminal) core,
             or write a minimal escape sequence parser over LunaGUI canvas
    Verdict: Selected for v1 — libvte-like core without GTK

  Decision: v1 uses a PTY-based terminal with a focused escape sequence
  parser (ANSI/VT100/VT220/xterm-256color) written as a LunaGUI canvas
  widget. Full xterm compatibility is not required for v1 — targeting
  the subset used by: bash, common CLI tools, neovim, tmux.
```

### Process Architecture

```
luna-terminal process:

  Main Thread:
    LunaGUI event loop
    ├── Keyboard events → write to PTY master
    ├── PTY output → parse escape sequences → render to canvas
    ├── Mouse events → selection, scrollback navigation
    └── Window resize → update PTY terminal size (TIOCSWINSZ)

  PTY Thread:
    └── Reads PTY master fd
        Parse VT escape sequences
        Update terminal cell grid (character + color + attributes)
        Signal render thread: dirty region

  Render Thread:
    └── On dirty signal: render dirty cells to LunaGUI canvas buffer
        Commit buffer to LGP surface
```

### Terminal Cell Grid

```c
typedef struct terminal_cell {
    uint32_t  codepoint;      // Unicode character
    uint32_t  fg_color;       // RGBA foreground
    uint32_t  bg_color;       // RGBA background
    uint8_t   bold     : 1;
    uint8_t   italic   : 1;
    uint8_t   underline: 1;
    uint8_t   blink    : 1;   // supported but defaulted to off
    uint8_t   inverse  : 1;   // fg/bg swap
    uint8_t   dirty    : 1;   // needs re-render
} terminal_cell_t;

// Terminal grid: rows × cols of cells
// Default: 80 columns × 24 rows (resizable with window)
terminal_cell_t grid[TERM_ROWS_MAX][TERM_COLS_MAX];
```

---

## LUNA Integration

### Command Observation Protocol

luna-terminal publishes D-Bus signals to `org.lunaos.context.Terminal`:

```
D-Bus signals published by luna-terminal:

  CommandExecuted:
    arguments:
      command:   string   — the command line that was executed
      cwd:       string   — working directory at time of execution
      timestamp: uint64   — Unix timestamp
    emitted: when user presses Enter to run a command

  CommandCompleted:
    arguments:
      command:    string
      exit_code:  int32
      duration_ms: uint32
      stderr_snippet: string  — first 200 chars of stderr (if exit_code ≠ 0)
    emitted: when command returns

  DirectoryChanged:
    arguments:
      new_cwd: string
    emitted: when the shell's working directory changes (cd, pushd)
```

**Privacy note:** Full command text is published to D-Bus. This means any D-Bus listener can see what the user types. In v1, only luna-ai-d subscribes to these signals. The permission model (observe.toml) governs whether luna-terminal is observed at all. If `luna-terminal` is not in `observe.toml`, these signals are still emitted but luna-ai-d ignores them.

### Error Detection

```python
# luna-ai-d side: handling CommandCompleted

def on_command_completed(command, exit_code, duration_ms, stderr_snippet):
    if exit_code == 0:
        # Success — update context, no action needed
        update_context(last_successful_command=command)
        return

    # Failure — check if this is a repeating error
    error_hash = hash_error(stderr_snippet)  # normalize + hash

    if is_repeating_error(error_hash, context.current_session):
        repeat_count = get_error_repeat_count(error_hash)
        # Presence Engine: is_repeating_error = True
        # Personality Engine will offer help if confidence passes
        emit_observation(
            type="REPEATED_BUILD_ERROR",
            count=repeat_count,
            command=command,
        )
```

---

## Configuration

```toml
# ~/.luna/config/terminal.toml

[terminal]
font_family   = "JetBrains Mono"   # Monospace font for terminal
font_size     = 13                  # pt
line_height   = 1.6
scrollback    = 10000               # lines of scrollback history
shell         = "/bin/bash"         # default shell
bell          = "none"              # "none" | "visual" | "audio"

[terminal.colors]
# Luna Dark color scheme for terminal
background    = "#0A0A0F"   # Void Black
foreground    = "#E8E8FF"   # Text Primary
cursor        = "#00E5A0"   # LUNA_GREEN cursor (alive feel)
selection_bg  = "#3D3D6B"   # Border Active

# ANSI 16 colors — Luna Dark palette
black         = "#0A0A0F"
red           = "#FF3CAC"   # LUNA_PINK (errors)
green         = "#00E5A0"   # LUNA_GREEN (success)
yellow        = "#FFB347"   # LUNA_AMBER (warnings)
blue          = "#4A9EFF"   # LUNA_BLUE
magenta       = "#C084FC"
cyan          = "#67E8F9"
white         = "#E8E8FF"
# Bright variants
bright_black  = "#3A3A5C"   # LUNA_VOID
bright_red    = "#FF6EC7"
bright_green  = "#34EFA0"
bright_yellow = "#FFC870"
bright_blue   = "#7BB8FF"
bright_magenta= "#D8A4FF"
bright_cyan   = "#93F0FF"
bright_white  = "#FFFFFF"

[terminal.keybindings]
new_tab       = "Ctrl+Shift+T"
close_tab     = "Ctrl+Shift+W"
next_tab      = "Ctrl+Tab"
prev_tab      = "Ctrl+Shift+Tab"
copy          = "Ctrl+Shift+C"
paste         = "Ctrl+Shift+V"
zoom_in       = "Ctrl+="
zoom_out      = "Ctrl+-"
zoom_reset    = "Ctrl+0"
search        = "Ctrl+Shift+F"
luna_assist   = "Ctrl+Shift+L"   # Open LUNA with terminal context
```

---

## Tab System

luna-terminal supports multiple tabs within a single window:

```
Tab bar (appears when > 1 tab open):

  ┌────────────────────────────────────────────────────────────┐
  │  [bash: ~/lunagui] ×  [bash: ~/lgp] ×  [+]               │
  │   ↑ Current tab         ↑ Other tab     ↑ New tab button  │
  └────────────────────────────────────────────────────────────┘

  Tab title format: "{shell}: {cwd_short}"
    cwd_short: last 2 path components (e.g. "~/lunagui/src" → "lunagui/src")
  
  Tab bar height: 28px
  Tab bar style:  matches luna-bar glass aesthetic
```

---

## LUNA Assist (Ctrl+Shift+L)

`Ctrl+Shift+L` opens the Luna Island FULL_CONVERSATION panel with the current terminal context pre-loaded:

```
LUNA Assist flow:

  1. User presses Ctrl+Shift+L in luna-terminal
  2. luna-terminal calls org.lunaos.luna.OpenConversationWithContext():
       context = {
           "trigger": "terminal_assist",
           "last_command": last_run_command,
           "last_exit_code": last_exit_code,
           "cwd": current_working_directory,
           "last_stderr": last_stderr_output,  // first 500 chars
       }
  3. luna-island transitions to FULL_CONVERSATION
  4. LUNA receives the context and opens with it pre-loaded:
       "Build failed in lunagui/src. Same error as 20 minutes ago.
        Want me to trace the shrink calculation?"
```

This is the primary DEVSHELL workflow: the user works in the terminal, hits an error, presses Ctrl+Shift+L, and LUNA is immediately in context without the user having to explain the situation.

---

## Multiplexer Support

luna-terminal is aware of terminal multiplexers (tmux, screen):

```
Multiplexer interaction:

  When tmux is detected (by process name in the PTY):
    → luna-terminal applies minimal multiplexer-aware theming
    → Passes TERM=xterm-256color
    → Passes COLORTERM=truecolor
    → Does NOT attempt to interpret tmux escape sequences
       (tmux handles its own UI inside the PTY)

  tmux clipboard integration:
    tmux's clipboard-write must be intercepted and forwarded to
    LunaOS clipboard (via LGP clipboard extension — DL-033).
    Required for: tmux copy-mode → system clipboard.
    Implementation: OSC 52 escape sequence support (terminal-level).
```

---

## Font Rendering

```
Terminal font rendering specification:

  Font:         Monospace (JetBrains Mono default, configurable)
  Renderer:     FreeType + HarfBuzz (DL-029) — same as LunaGUI
  Subpixel:     ClearType-equivalent (RGB subpixel AA on LCD displays)
  Emoji:        Color emoji via FreeType emoji table (Noto Color Emoji)

  Font size variants rendered at startup:
    Normal:  13pt
    Bold:    13pt bold (different weight, not scaled)
    Italic:  13pt italic (slanted variant)
    BoldItalic: 13pt bold italic

  Ligature support:
    Enabled by default for supported fonts (JetBrains Mono has ligatures)
    Can be disabled in terminal.toml: ligatures = false
```

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| Input echo latency (keypress → visible char) | < 8ms | 16ms |
| Scrollback scroll frame rate | 60 fps | 30 fps min |
| Full screen redraw (80×24, all cells dirty) | < 4ms | 8ms |
| PTY read → cell grid update | < 1ms | 3ms |
| RAM usage (single tab, idle) | < 30 MB | 60 MB |
| RAM per additional tab | < 5 MB | 15 MB |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| luna-terminal is a standard LunaGUI APPLICATION_WINDOW | This document | ✅ Accepted |
| v1 uses PTY + focused escape sequence parser (no VTE/GTK) | This document | ✅ Accepted |
| Command observation via D-Bus (opt-in per observe.toml) | DL-022 | ✅ Accepted |
| Terminal color scheme: Luna Dark palette | Volume III/09 | ✅ Accepted |
| LUNA_GREEN cursor (alive aesthetic) | Volume III/09 | ✅ Accepted |
| LUNA Assist: Ctrl+Shift+L | This document | ✅ Accepted |
| Default shell: bash | This document | 🧪 Experimental |
| OSC 52 clipboard (tmux integration) | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Default shell.** bash is specified as default. Should it be bash, zsh, or fish? Many developer-focused distros default to zsh. The choice affects the default prompt and scripting behavior. Must be a Decision Log entry.

2. **Escape sequence coverage.** The spec says "subset used by bash, common CLI tools, neovim, tmux." The exact set of escape sequences to support in v1 must be enumerated before implementation. neovim in particular pushes the limits of terminal emulation (Kitty graphics protocol, etc.).

3. **GPU-accelerated terminal.** Popular terminal emulators (Alacritty, kitty, WezTerm) use GPU rendering for performance. Should luna-terminal use a GPU-rendered canvas for text? This would require the Vulkan path (Stage 3+). For v1 (CPU renderer), the performance budget must be met without GPU.

4. **Shell integration scripts.** Tools like starship prompt, zoxide, and fzf need shell integration scripts to work properly. Does LunaOS ship default shell integration scripts for common tools? Must be specified before the installer is written.

5. **Command privacy.** Full command text is published to D-Bus. Commands may contain passwords (e.g., `mysql -p secretpassword`). Should luna-terminal detect and redact sensitive patterns before publishing? A simple heuristic (redact after `-p`, `--password=`, etc.) would help. Must be a Decision Log entry.

---

## AI Context

- luna-terminal is the **primary DEVSHELL context source**. When the Presence Engine is in DEVSHELL mode, most of its signal comes from luna-terminal's D-Bus publications. If luna-terminal is not running, DEVSHELL mode is less accurate.
- The command observation D-Bus signal includes the full command text. Be cautious about what is logged from these signals. The audit log should not store full command strings — only the classified context (CODING) and exit status.
- LUNA Assist (`Ctrl+Shift+L`) is the showcase integration feature. It must be fast — the context pre-loading in luna-island should happen in < 500ms from keypress.
- The terminal color scheme uses semantic colors: LUNA_PINK for `red` (errors), LUNA_GREEN for `green` (success), LUNA_AMBER for `yellow` (warnings). This is intentional — the semantic color contract (Volume III/09) applies here too, making the terminal visually consistent with the rest of the OS.
- Terminal input latency is user-perception-critical. The `< 8ms` keypress-to-echo target is not negotiable for a developer tool. Any rendering optimization that increases this latency is unacceptable.

---

*Document: `Volume V / 02_terminal.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume III/04_lunagui.md, Volume III/09_visual_language.md, Volume IV/01_presence_engine.md, DL-022, DL-029*
*Informs: Volume V/08_sdk.md*
