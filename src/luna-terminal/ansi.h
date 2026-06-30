/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * ansi.h — ANSI/VT100 escape sequence parser for luna-terminal.
 *
 * Parses incoming byte streams from the PTY and produces structured
 * TermEvent values that the terminal renders into the cell grid.
 *
 * Supported sequences (VT100/ECMA-48 subset):
 *   \x1b[<n>A/B/C/D        — cursor movement
 *   \x1b[<row>;<col>H      — cursor position
 *   \x1b[<n>J              — erase in display (0=below, 1=above, 2=all)
 *   \x1b[<n>K              — erase in line (0=to end, 1=to start, 2=all)
 *   \x1b[<n>m              — SGR: color, bold, reset
 *   \x1b[?25h/l            — cursor show/hide
 *   \r \n \b \t            — control characters
 */

#ifndef LUNA_TERMINAL_ANSI_H
#define LUNA_TERMINAL_ANSI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Cell representation ────────────────────────────────────────────────── */

typedef struct term_cell_t {
    uint32_t ch;        /* Unicode codepoint (ASCII for now) */
    uint32_t fg;        /* Foreground XRGB8888 */
    uint32_t bg;        /* Background XRGB8888 */
    bool     bold;
    bool     dirty;     /* Needs repaint */
} term_cell_t;

/* ── ANSI SGR color table ───────────────────────────────────────────────── */

/* Standard 8-color ANSI palette (matches typical dark-mode terminals) */
static const uint32_t ANSI_COLORS[16] = {
    0xFF1E1E28u, /* 0: Black       */
    0xFFCC4444u, /* 1: Red         */
    0xFF44CC66u, /* 2: Green       */
    0xFFCCBB33u, /* 3: Yellow      */
    0xFF4488CCu, /* 4: Blue        */
    0xFFAA44AAu, /* 5: Magenta     */
    0xFF44AACC u, /* 6: Cyan       */
    0xFFBBBBCCu, /* 7: White       */
    /* Bright variants */
    0xFF555566u, /* 8:  Bright Black   */
    0xFFFF6666u, /* 9:  Bright Red     */
    0xFF66FF88u, /* 10: Bright Green   */
    0xFFFFEE55u, /* 11: Bright Yellow  */
    0xFF66AAFFu, /* 12: Bright Blue    */
    0xFFCC66CCu, /* 13: Bright Magenta */
    0xFF66CCFFu, /* 14: Bright Cyan    */
    0xFFEEEEFFu, /* 15: Bright White   */
};

#define ANSI_DEFAULT_FG 0xFFEEEEF4u
#define ANSI_DEFAULT_BG 0xFF1E1E28u

/* ── Parser state ───────────────────────────────────────────────────────── */

#define ANSI_MAX_PARAMS 8

typedef enum {
    ANSI_STATE_GROUND = 0,
    ANSI_STATE_ESCAPE,
    ANSI_STATE_CSI,         /* Control Sequence Introducer */
    ANSI_STATE_CSI_PARAM,
} ansi_state_t;

typedef struct ansi_parser_t {
    ansi_state_t state;

    /* CSI parameter accumulation */
    int  params[ANSI_MAX_PARAMS];
    int  param_count;
    int  cur_param;

    /* Private marker (e.g. '?' in \x1b[?25h) */
    bool private_marker;

    /* Current text attributes */
    uint32_t fg;
    uint32_t bg;
    bool     bold;
} ansi_parser_t;

/* ── Terminal grid ──────────────────────────────────────────────────────── */

#define TERM_COLS_MAX  220
#define TERM_ROWS_MAX   60

typedef struct term_grid_t {
    int cols;
    int rows;
    int cursor_col;
    int cursor_row;
    bool cursor_visible;

    term_cell_t cells[TERM_ROWS_MAX][TERM_COLS_MAX];

    /* Scrollback buffer (circular) */
    term_cell_t scrollback[2000][TERM_COLS_MAX];
    int         scrollback_len;
    int         scrollback_head;

    /* Scroll offset for display (0 = bottom, positive = scroll up) */
    int scroll_offset;
} term_grid_t;

/* ── API ────────────────────────────────────────────────────────────────── */

void ansi_parser_init(ansi_parser_t *p);
void term_grid_init(term_grid_t *g, int cols, int rows);
void term_grid_resize(term_grid_t *g, int new_cols, int new_rows);

/*
 * ansi_feed() — Feed a buffer of raw bytes from the PTY into the parser.
 * Processes escape sequences and updates the grid in place.
 */
void ansi_feed(ansi_parser_t *p, term_grid_t *g,
               const uint8_t *data, size_t len);

#endif
