/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * ansi.c — ANSI/VT100 escape sequence parser implementation.
 *
 * State machine: GROUND → ESCAPE → CSI → CSI_PARAM → (back to GROUND)
 *
 * On each printable character in GROUND state, the character is written to
 * the current cursor cell and the cursor advances. On wrap, a new row is
 * created and the top line is moved to the scrollback buffer.
 */

#include "ansi.h"
#include <string.h>
#include <stdlib.h>

/* ── Utilities ────────────────────────────────────────────────────────────── */

static void cell_clear(term_cell_t *c, uint32_t bg) {
    c->ch    = ' ';
    c->fg    = ANSI_DEFAULT_FG;
    c->bg    = bg;
    c->bold  = false;
    c->dirty = true;
}

static void row_clear(term_grid_t *g, int row, uint32_t bg) {
    for (int c = 0; c < g->cols; c++) {
        cell_clear(&g->cells[row][c], bg);
    }
}

/* ── Public init ──────────────────────────────────────────────────────────── */

void ansi_parser_init(ansi_parser_t *p) {
    memset(p, 0, sizeof(*p));
    p->state = ANSI_STATE_GROUND;
    p->fg    = ANSI_DEFAULT_FG;
    p->bg    = ANSI_DEFAULT_BG;
    p->bold  = false;
}

void term_grid_init(term_grid_t *g, int cols, int rows) {
    if (cols > TERM_COLS_MAX) cols = TERM_COLS_MAX;
    if (rows > TERM_ROWS_MAX) rows = TERM_ROWS_MAX;
    g->cols           = cols;
    g->rows           = rows;
    g->cursor_col     = 0;
    g->cursor_row     = 0;
    g->cursor_visible = true;
    g->scrollback_len = 0;
    g->scrollback_head = 0;
    g->scroll_offset  = 0;

    for (int r = 0; r < rows; r++) {
        row_clear(g, r, ANSI_DEFAULT_BG);
    }
}

void term_grid_resize(term_grid_t *g, int new_cols, int new_rows) {
    if (new_cols > TERM_COLS_MAX) new_cols = TERM_COLS_MAX;
    if (new_rows > TERM_ROWS_MAX) new_rows = TERM_ROWS_MAX;
    if (new_cols <= 0 || new_rows <= 0) return;

    /* Clear any newly exposed rows */
    for (int r = g->rows; r < new_rows; r++) {
        row_clear(g, r, ANSI_DEFAULT_BG);
    }

    /* Clamp cursor */
    if (g->cursor_col >= new_cols) g->cursor_col = new_cols - 1;
    if (g->cursor_row >= new_rows) g->cursor_row = new_rows - 1;

    g->cols = new_cols;
    g->rows = new_rows;
}

/* ── Scrollback helpers ───────────────────────────────────────────────────── */

/* Move the top row of the display into the scrollback buffer and scroll up */
static void scroll_up(term_grid_t *g) {
    int max_scrollback = (int)(sizeof(g->scrollback) / sizeof(g->scrollback[0]));

    /* Copy first row into scrollback */
    memcpy(g->scrollback[g->scrollback_head], g->cells[0],
           (size_t)g->cols * sizeof(term_cell_t));
    g->scrollback_head = (g->scrollback_head + 1) % max_scrollback;
    if (g->scrollback_len < max_scrollback) g->scrollback_len++;

    /* Shift display rows up */
    for (int r = 1; r < g->rows; r++) {
        memcpy(g->cells[r - 1], g->cells[r],
               (size_t)g->cols * sizeof(term_cell_t));
    }
    row_clear(g, g->rows - 1, ANSI_DEFAULT_BG);
}

/* ── Character output ─────────────────────────────────────────────────────── */

static void put_char(ansi_parser_t *p, term_grid_t *g, uint32_t ch) {
    if (g->cursor_row >= g->rows) {
        g->cursor_row = g->rows - 1;
    }
    if (g->cursor_col >= g->cols) {
        /* Wrap */
        g->cursor_col = 0;
        g->cursor_row++;
        if (g->cursor_row >= g->rows) {
            scroll_up(g);
            g->cursor_row = g->rows - 1;
        }
    }

    term_cell_t *cell = &g->cells[g->cursor_row][g->cursor_col];
    cell->ch    = ch;
    cell->fg    = p->bold ? (p->fg | 0x404040u) : p->fg;
    cell->bg    = p->bg;
    cell->bold  = p->bold;
    cell->dirty = true;

    g->cursor_col++;
}

/* ── Control characters ───────────────────────────────────────────────────── */

static void handle_control(ansi_parser_t *p, term_grid_t *g, uint8_t ch) {
    (void)p;
    switch (ch) {
        case '\r':
            g->cursor_col = 0;
            break;
        case '\n':
            g->cursor_row++;
            if (g->cursor_row >= g->rows) {
                scroll_up(g);
                g->cursor_row = g->rows - 1;
            }
            break;
        case '\b':
            if (g->cursor_col > 0) g->cursor_col--;
            break;
        case '\t': {
            int next_tab = ((g->cursor_col / 8) + 1) * 8;
            if (next_tab >= g->cols) next_tab = g->cols - 1;
            g->cursor_col = next_tab;
            break;
        }
        default:
            break;
    }
}

/* ── SGR (Select Graphic Rendition) ──────────────────────────────────────── */

static void apply_sgr(ansi_parser_t *p, int *params, int count) {
    if (count == 0) {
        /* ESC[m = reset */
        p->fg   = ANSI_DEFAULT_FG;
        p->bg   = ANSI_DEFAULT_BG;
        p->bold = false;
        return;
    }

    for (int i = 0; i < count; i++) {
        int v = params[i];
        if (v == 0) {
            p->fg   = ANSI_DEFAULT_FG;
            p->bg   = ANSI_DEFAULT_BG;
            p->bold = false;
        } else if (v == 1) {
            p->bold = true;
        } else if (v == 22) {
            p->bold = false;
        } else if (v >= 30 && v <= 37) {
            p->fg = ANSI_COLORS[v - 30];
        } else if (v == 39) {
            p->fg = ANSI_DEFAULT_FG;
        } else if (v >= 40 && v <= 47) {
            p->bg = ANSI_COLORS[v - 40];
        } else if (v == 49) {
            p->bg = ANSI_DEFAULT_BG;
        } else if (v >= 90 && v <= 97) {
            p->fg = ANSI_COLORS[v - 90 + 8];
        } else if (v >= 100 && v <= 107) {
            p->bg = ANSI_COLORS[v - 100 + 8];
        }
    }
}

/* ── CSI dispatch ─────────────────────────────────────────────────────────── */

static int param_or(int *params, int idx, int count, int def) {
    if (idx < count && params[idx] > 0) return params[idx];
    return def;
}

static void dispatch_csi(ansi_parser_t *p, term_grid_t *g,
                          char final, bool priv,
                          int *params, int count) {
    (void)priv;
    switch (final) {
        case 'A': /* Cursor Up */
            g->cursor_row -= param_or(params, 0, count, 1);
            if (g->cursor_row < 0) g->cursor_row = 0;
            break;
        case 'B': /* Cursor Down */
            g->cursor_row += param_or(params, 0, count, 1);
            if (g->cursor_row >= g->rows) g->cursor_row = g->rows - 1;
            break;
        case 'C': /* Cursor Forward */
            g->cursor_col += param_or(params, 0, count, 1);
            if (g->cursor_col >= g->cols) g->cursor_col = g->cols - 1;
            break;
        case 'D': /* Cursor Back */
            g->cursor_col -= param_or(params, 0, count, 1);
            if (g->cursor_col < 0) g->cursor_col = 0;
            break;
        case 'H': /* Cursor Position: ESC[row;colH (1-indexed) */
        case 'f': {
            int row = param_or(params, 0, count, 1) - 1;
            int col = param_or(params, 1, count, 1) - 1;
            if (row < 0) row = 0;
            if (col < 0) col = 0;
            if (row >= g->rows) row = g->rows - 1;
            if (col >= g->cols) col = g->cols - 1;
            g->cursor_row = row;
            g->cursor_col = col;
            break;
        }
        case 'J': /* Erase in Display */
            switch (param_or(params, 0, count, 0)) {
                case 0: /* Erase below */
                    for (int c = g->cursor_col; c < g->cols; c++)
                        cell_clear(&g->cells[g->cursor_row][c], p->bg);
                    for (int r = g->cursor_row + 1; r < g->rows; r++)
                        row_clear(g, r, p->bg);
                    break;
                case 1: /* Erase above */
                    for (int c = 0; c <= g->cursor_col; c++)
                        cell_clear(&g->cells[g->cursor_row][c], p->bg);
                    for (int r = 0; r < g->cursor_row; r++)
                        row_clear(g, r, p->bg);
                    break;
                case 2: /* Erase all */
                    for (int r = 0; r < g->rows; r++)
                        row_clear(g, r, p->bg);
                    g->cursor_row = 0;
                    g->cursor_col = 0;
                    break;
            }
            break;
        case 'K': /* Erase in Line */
            switch (param_or(params, 0, count, 0)) {
                case 0: /* Erase to end of line */
                    for (int c = g->cursor_col; c < g->cols; c++)
                        cell_clear(&g->cells[g->cursor_row][c], p->bg);
                    break;
                case 1: /* Erase to start of line */
                    for (int c = 0; c <= g->cursor_col; c++)
                        cell_clear(&g->cells[g->cursor_row][c], p->bg);
                    break;
                case 2: /* Erase line */
                    row_clear(g, g->cursor_row, p->bg);
                    break;
            }
            break;
        case 'm': /* SGR */
            apply_sgr(p, params, count);
            break;
        case 'h': /* DEC Private Mode Set */
            if (priv && count == 1 && params[0] == 25) {
                g->cursor_visible = true;
            }
            break;
        case 'l': /* DEC Private Mode Reset */
            if (priv && count == 1 && params[0] == 25) {
                g->cursor_visible = false;
            }
            break;
        /* Ignore unsupported sequences */
        default:
            break;
    }
}

/* ── Main byte feeder ─────────────────────────────────────────────────────── */

void ansi_feed(ansi_parser_t *p, term_grid_t *g,
               const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = data[i];

        switch (p->state) {

        case ANSI_STATE_GROUND:
            if (ch == 0x1Bu) {
                p->state = ANSI_STATE_ESCAPE;
            } else if (ch < 0x20u) {
                handle_control(p, g, ch);
            } else if (ch >= 0x20u && ch < 0x7Fu) {
                put_char(p, g, ch);
            }
            /* Ignore DEL (0x7F) and high bytes for now */
            break;

        case ANSI_STATE_ESCAPE:
            if (ch == '[') {
                p->state          = ANSI_STATE_CSI;
                p->param_count    = 0;
                p->cur_param      = 0;
                p->private_marker = false;
                memset(p->params, 0, sizeof(p->params));
            } else if (ch == 'c') {
                /* RIS: Full reset */
                term_grid_init(g, g->cols, g->rows);
                ansi_parser_init(p);
                p->state = ANSI_STATE_GROUND;
            } else {
                /* Unknown escape — return to ground */
                p->state = ANSI_STATE_GROUND;
            }
            break;

        case ANSI_STATE_CSI:
            if (ch == '?') {
                p->private_marker = true;
            } else if (ch >= '0' && ch <= '9') {
                p->cur_param = ch - '0';
                p->state = ANSI_STATE_CSI_PARAM;
            } else if (ch == ';') {
                /* Empty first parameter */
                if (p->param_count < ANSI_MAX_PARAMS)
                    p->params[p->param_count++] = 0;
            } else if (ch >= 0x40u && ch <= 0x7Eu) {
                /* Final byte with no parameters */
                dispatch_csi(p, g, (char)ch, p->private_marker,
                              p->params, p->param_count);
                p->state = ANSI_STATE_GROUND;
            } else {
                p->state = ANSI_STATE_GROUND;
            }
            break;

        case ANSI_STATE_CSI_PARAM:
            if (ch >= '0' && ch <= '9') {
                p->cur_param = p->cur_param * 10 + (ch - '0');
            } else if (ch == ';') {
                if (p->param_count < ANSI_MAX_PARAMS)
                    p->params[p->param_count++] = p->cur_param;
                p->cur_param = 0;
            } else if (ch >= 0x40u && ch <= 0x7Eu) {
                /* Final byte */
                if (p->param_count < ANSI_MAX_PARAMS)
                    p->params[p->param_count++] = p->cur_param;
                dispatch_csi(p, g, (char)ch, p->private_marker,
                              p->params, p->param_count);
                p->state = ANSI_STATE_GROUND;
            } else {
                p->state = ANSI_STATE_GROUND;
            }
            break;
        }
    }
}
