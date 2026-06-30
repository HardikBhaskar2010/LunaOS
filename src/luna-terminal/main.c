/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * luna-terminal — MahinaOS graphical terminal emulator.
 *
 * Architecture:
 *   ┌───────────────────────────────────────────────────────────┐
 *   │  /bin/sh (child)  ←→  PTY master (pty_fd)                │
 *   │  pty_fd registered with lgui_application_add_fd()        │
 *   │  On readable: ansi_feed() → term_grid_t → render_grid()  │
 *   │  On window resize: SIGWINCH via ioctl(TIOCSWINSZ)        │
 *   └───────────────────────────────────────────────────────────┘
 *
 * Font: The embedded PSF font is 8x16 pixels.
 * Window: 80×24 = 640×384 (plus 4px padding each side → 648×392)
 *
 * Phase D requirements met:
 *   [x] PTY (forkpty)
 *   [x] ANSI escape sequences (ansi.c)
 *   [x] Scrollback (2000 lines in term_grid_t)
 *   [x] PTY resize via TIOCSWINSZ
 *   [ ] Copy/paste — deferred until clipboard LGP message added (DL-??? TBD)
 *   [ ] Multiple sessions — deferred (need tabbar widget first)
 */

#include "lunagui.h"
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

/* ── Terminal configuration ──────────────────────────────────────────────── */

#define TERM_FONT_W      8    /* PSF glyph width in pixels  */
#define TERM_FONT_H     16    /* PSF glyph height in pixels */
#define TERM_COLS       80
#define TERM_ROWS       24
#define TERM_PAD         4    /* Pixel padding around the grid */

#define TERM_WIN_W  (TERM_COLS * TERM_FONT_W + TERM_PAD * 2)
#define TERM_WIN_H  (TERM_ROWS * TERM_FONT_H + TERM_PAD * 2)

/* ── Global state ────────────────────────────────────────────────────────── */

static lgui_application_t *g_app = NULL;
static lgui_window_t      *g_win = NULL;
static ansi_parser_t       g_parser;
static term_grid_t         g_grid;
static int                 g_pty_fd = -1;

/* ── PTY spawn ───────────────────────────────────────────────────────────── */

int pty_spawn(pid_t *out_pid, int *out_fd);

/* ── Rendering ───────────────────────────────────────────────────────────── */

/*
 * render_grid() — Walk the dirty cells in g_grid and repaint them into the
 * window's pixel buffer, then commit the frame to the compositor.
 *
 * Uses lgui_canvas_t to access the raw pixel buffer. For each cell:
 *   1. Fill background rectangle
 *   2. Draw character using lgui_canvas_draw_text()
 */
static void terminal_render_cb(lgui_widget_t *widget, lgui_canvas_t *canvas, int x, int y) {
    (void)widget;
    
    for (int r = 0; r < g_grid.rows; r++) {
        for (int c = 0; c < g_grid.cols; c++) {
            term_cell_t cell = g_grid.cells[r][c];
            
            int cell_x = x + TERM_PAD + c * TERM_FONT_W;
            int cell_y = y + TERM_PAD + r * TERM_FONT_H;

            /* Default colors */
            uint32_t bg = 0xFF1E1E28u; /* MAHINA_BG_SURFACE */
            uint32_t fg = 0xFFEEEEF4u; /* MAHINA_TEXT_PRIMARY */
            
            /* TODO: parse cell.fg and cell.bg ANSI colors if needed */
            if (cell.flags & TERM_CELL_INVERSE) {
                uint32_t tmp = bg; bg = fg; fg = tmp;
            }

            /* Draw background */
            if (bg != 0xFF1E1E28u) {
                lgui_canvas_fill_rect(canvas, cell_x, cell_y, TERM_FONT_W, TERM_FONT_H, bg);
            }

            /* Draw foreground character */
            if (cell.ch >= 0x20u && cell.ch < 0x7Fu) {
                char str[2] = { (char)cell.ch, '\0' };
                lgui_canvas_draw_text(canvas, cell_x, cell_y, str, fg);
            }
        }
    }
}

static void render_grid(void) {
    if (g_win) {
        g_win->dirty = true;
        /* Trigger a re-render in the event loop */
    }
}

/* ── PTY callback ────────────────────────────────────────────────────────── */

static void pty_callback(int fd, void *user_data) {
    (void)user_data;

    uint8_t buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        ansi_feed(&g_parser, &g_grid, buf, (size_t)n);
        render_grid();
    } else if (n == 0 || (n < 0)) {
        /* Shell exited */
        lgui_application_quit(g_app);
    }
}

/* ── PTY resize ──────────────────────────────────────────────────────────── */

static void pty_resize(int cols, int rows) {
    if (g_pty_fd < 0) return;

    struct winsize ws;
    ws.ws_col    = (unsigned short)cols;
    ws.ws_row    = (unsigned short)rows;
    ws.ws_xpixel = (unsigned short)(cols * TERM_FONT_W);
    ws.ws_ypixel = (unsigned short)(rows * TERM_FONT_H);
    ioctl(g_pty_fd, TIOCSWINSZ, &ws);

    term_grid_resize(&g_grid, cols, rows);
}

#include <linux/input-event-codes.h>

static void terminal_on_paste(const char *text, void *user_data) {
    (void)user_data;
    if (text && g_pty_fd >= 0) {
        write(g_pty_fd, text, strlen(text));
    }
}

static void terminal_on_key(lgui_widget_t *widget, uint32_t key, uint32_t modifiers, void *user_data) {
    (void)widget;
    (void)user_data;
    if (g_pty_fd < 0) return;

    /* Ctrl+Shift+C / Ctrl+Shift+V */
    if (modifiers == 3) { /* Shift | Ctrl */
        if (key == KEY_C) {
            /* TODO: Copy selected text */
            return;
        } else if (key == KEY_V) {
            lgui_clipboard_request_text(g_app, terminal_on_paste, NULL);
            return;
        }
    }

    /* Handle special keys with ANSI escape sequences */
    const char *seq = NULL;
    if (key == KEY_UP)    seq = "\x1b[A";
    if (key == KEY_DOWN)  seq = "\x1b[B";
    if (key == KEY_RIGHT) seq = "\x1b[C";
    if (key == KEY_LEFT)  seq = "\x1b[D";
    
    if (seq) {
        write(g_pty_fd, seq, strlen(seq));
        return;
    }

    /* Translate normal keys */
    char c = lgui_keymap_translate(key, modifiers);
    if (c != '\0') {
        write(g_pty_fd, &c, 1);
    }
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void) {
    g_app = lgui_application_create("luna-terminal");
    if (!g_app) {
        fprintf(stderr, "luna-terminal: cannot connect to lgp-compositor\n");
        return 1;
    }

    /* Initialize terminal state */
    ansi_parser_init(&g_parser);
    term_grid_init(&g_grid, TERM_COLS, TERM_ROWS);

    /* Spawn PTY + shell */
    pid_t shell_pid = -1;
    if (pty_spawn(&shell_pid, &g_pty_fd) != 0 || g_pty_fd < 0) {
        fprintf(stderr, "luna-terminal: pty_spawn failed\n");
        lgui_application_destroy(g_app);
        return 1;
    }

    /* Set initial PTY window size */
    pty_resize(TERM_COLS, TERM_ROWS);

    /* Register PTY fd with the event loop */
    lgui_application_add_fd(g_app, g_pty_fd, pty_callback, NULL);

    /* Create window */
    g_win = lgui_window_create(g_app, TERM_WIN_W, TERM_WIN_H,
                                LGUI_LAYER_APPLICATION);
    if (!g_win) {
        fprintf(stderr, "luna-terminal: cannot create window\n");
        lgui_application_destroy(g_app);
        return 1;
    }

    /* Set up terminal UI using a CanvasWidget */
    lgui_widget_t *canvas_widget = lgui_canvas_widget_create();
    lgui_widget_set_size(canvas_widget, TERM_WIN_W, TERM_WIN_H);
    lgui_canvas_widget_set_render(canvas_widget, terminal_render_cb);
    lgui_widget_set_on_key(canvas_widget, terminal_on_key, NULL);

    lgui_window_set_root_widget(g_win, canvas_widget);
    lgui_widget_focus(g_app, canvas_widget);

    /* Initial render */
    render_grid();
    lgui_window_show(g_win);

    lgui_application_run(g_app);
    lgui_application_destroy(g_app);

    /* Clean up shell process */
    if (shell_pid > 0) {
        kill(shell_pid, SIGHUP);
    }

    return 0;
}
