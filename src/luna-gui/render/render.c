/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * render.c — LunaGUI widget renderer.
 *
 * Traverses the widget tree and renders each widget into a canvas using the
 * Mahina design system color palette. Layout is computed top-down before
 * rendering so parent constraints propagate to children correctly.
 *
 * Widget types:
 *   1 = Label     — text-only, transparent background
 *   2 = Button    — filled background + border + text
 *   3 = VBox      — vertical stack layout
 *   4 = HBox      — horizontal stack layout
 */

#include "lunagui.h"
#include "widget_private.h"

/* ── Mahina Design System Color Palette ────────────────────────────────────── */

/* Backgrounds */
#define MAHINA_BG_SURFACE    0xFF1E1E28u  /* Primary dark surface */
#define MAHINA_BG_RAISED     0xFF262636u  /* Raised element (e.g. button base) */
#define MAHINA_BG_HOVER      0xFF2E2E42u  /* Hover state */
#define MAHINA_BG_PRESSED    0xFF181824u  /* Active/pressed state */

/* Accents */
#define MAHINA_ACCENT_PRIMARY 0xFF6B7FD4u  /* Soft indigo accent */
#define MAHINA_ACCENT_GLOW    0xFF8A9FFFu  /* Brighter highlight */

/* Text */
#define MAHINA_TEXT_PRIMARY   0xFFEEEEF4u  /* Primary text — near white */
#define MAHINA_TEXT_SECONDARY 0xFF9898AAu  /* Secondary/muted text */
#define MAHINA_TEXT_DISABLED  0xFF555566u  /* Disabled text */

/* Borders */
#define MAHINA_BORDER_SUBTLE  0xFF303044u  /* Subtle divider */
#define MAHINA_BORDER_ACCENT  0xFF4A5CA0u  /* Accent border */

/* ── Layout helpers ───────────────────────────────────────────────────────── */

/* Compute layout for a VBox: distribute children vertically.
 * Each child gets full width; height is fixed per-child (default 32px for
 * regular widgets, 20px for labels). */
static void layout_vbox(lgui_widget_t *box) {
    int y_cursor = 4; /* 4px top padding */
    const int h_padding = 4;

    for (int i = 0; i < box->child_count; i++) {
        lgui_widget_t *child = box->children[i];
        child->x = 8; /* 8px left margin */
        child->y = y_cursor;

        if (child->width == 0) {
            child->width = box->width > 16 ? box->width - 16 : box->width;
        }
        if (child->height == 0) {
            child->height = (child->type == 1 /* label */) ? 20 : 32;
        }

        y_cursor += child->height + h_padding;
    }
}

/* Compute layout for an HBox: distribute children horizontally.
 * Children with width == 0 share the remaining space evenly. */
static void layout_hbox(lgui_widget_t *box) {
    /* Count flexible children */
    int fixed_w  = 0;
    int flex_cnt = 0;
    for (int i = 0; i < box->child_count; i++) {
        if (box->children[i]->width == 0) {
            flex_cnt++;
        } else {
            fixed_w += box->children[i]->width;
        }
    }

    int flex_w = flex_cnt > 0
               ? (box->width - fixed_w - 8) / flex_cnt
               : 0;

    int x_cursor = 4;
    for (int i = 0; i < box->child_count; i++) {
        lgui_widget_t *child = box->children[i];
        child->x = x_cursor;
        child->y = (box->height > 0) ? (box->height - 30) / 2 : 4;

        if (child->width == 0) child->width  = flex_w > 0 ? flex_w : 80;
        if (child->height == 0) child->height = box->height > 8 ? box->height - 8 : 24;

        x_cursor += child->width + 4;
    }
}

/* ── Widget renderers ─────────────────────────────────────────────────────── */

static void render_label(lgui_canvas_t *canvas, const lgui_widget_t *w,
                          int cx, int cy) {
    lgui_canvas_draw_text(canvas, cx, cy + 2, w->text, MAHINA_TEXT_PRIMARY);
}

static void render_button(lgui_canvas_t *canvas, const lgui_widget_t *w,
                           int cx, int cy) {
    /* Button body */
    lgui_canvas_fill_rect(canvas, cx, cy, w->width, w->height, MAHINA_BG_RAISED);

    /* Subtle accent border */
    lgui_canvas_draw_rect_outline(canvas, cx, cy, w->width, w->height,
                                   MAHINA_BORDER_ACCENT);

    /* Top highlight (simulates slight top-light) */
    lgui_canvas_fill_rect(canvas, cx + 1, cy + 1, w->width - 2, 1,
                           MAHINA_BG_HOVER);

    /* Centered text */
    int text_x = cx + 10;
    int text_y = cy + (w->height - 16) / 2; /* vertically center 16px glyph */
    lgui_canvas_draw_text(canvas, text_x, text_y, w->text, MAHINA_TEXT_PRIMARY);
}

/* ── Main recursive renderer ──────────────────────────────────────────────── */

void lgui_widget_render(lgui_canvas_t *canvas, lgui_widget_t *w,
                         int offset_x, int offset_y) {
    if (!w || !canvas) return;

    int cx = offset_x + w->x;
    int cy = offset_y + w->y;

    switch (w->type) {
        case 1: /* Label */
            render_label(canvas, w, cx, cy);
            break;
        case 2: /* Button */
            render_button(canvas, w, cx, cy);
            break;
        case 3: /* VBox */
            layout_vbox(w);
            for (int i = 0; i < w->child_count; i++) {
                lgui_widget_render(canvas, w->children[i], cx, cy);
            }
            break;
        case 4: /* HBox */
            layout_hbox(w);
            for (int i = 0; i < w->child_count; i++) {
                lgui_widget_render(canvas, w->children[i], cx, cy);
            }
            break;
        case 5: /* Scroll Container */
            if (w->child_count > 0) {
                lgui_widget_t *child = w->children[0];
                child->width = w->width;
                /* Height depends on layout, but for VBox we can just let it layout freely */
                lgui_canvas_push_clip(canvas, cx, cy, w->width, w->height);
                lgui_widget_render(canvas, child, cx - w->scroll_offset_x, cy - w->scroll_offset_y);
                lgui_canvas_pop_clip(canvas);
            }
            break;
        case 6: /* Canvas Widget */
            if (w->canvas_render) {
                lgui_canvas_push_clip(canvas, cx, cy, w->width, w->height);
                w->canvas_render(w, canvas, cx, cy);
                lgui_canvas_pop_clip(canvas);
            }
            break;
        default:
            /* Unknown widget type — still recurse into children */
            for (int i = 0; i < w->child_count; i++) {
                lgui_widget_render(canvas, w->children[i], cx, cy);
            }
            break;
    }
}
