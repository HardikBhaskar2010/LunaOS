/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "lunagui.h"
#include <string.h>

#include "lgui_private.h"
#include <stdlib.h>

static bool lgui_canvas_get_clip(lgui_canvas_t *canvas, int *cx, int *cy, int *cw, int *ch) {
    if (!canvas || canvas->clip_count == 0) return false;
    lgui_clip_rect_t *r = &canvas->clip_stack[canvas->clip_count - 1];
    *cx = r->x;
    *cy = r->y;
    *cw = r->w;
    *ch = r->h;
    return true;
}

void lgui_canvas_push_clip(lgui_canvas_t *canvas, int x, int y, int w, int h) {
    if (!canvas || canvas->clip_count >= LGUI_MAX_CLIP_STACK) return;

    /* Intersect with current top clip if any */
    if (canvas->clip_count > 0) {
        lgui_clip_rect_t *top = &canvas->clip_stack[canvas->clip_count - 1];
        int nx = x > top->x ? x : top->x;
        int ny = y > top->y ? y : top->y;
        int nw = (x + w < top->x + top->w ? x + w : top->x + top->w) - nx;
        int nh = (y + h < top->y + top->h ? y + h : top->y + top->h) - ny;
        if (nw < 0) nw = 0;
        if (nh < 0) nh = 0;
        x = nx; y = ny; w = nw; h = nh;
    }

    lgui_clip_rect_t *r = &canvas->clip_stack[canvas->clip_count++];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

void lgui_canvas_pop_clip(lgui_canvas_t *canvas) {
    if (!canvas || canvas->clip_count <= 1) return; /* Cannot pop the base clip */
    canvas->clip_count--;
}

void lgui_canvas_fill_rect(lgui_canvas_t *canvas, int x, int y, int w, int h, uint32_t color) {
    if (!canvas || !canvas->pixels) return;
    
    int cx, cy, cw, ch;
    if (!lgui_canvas_get_clip(canvas, &cx, &cy, &cw, &ch)) return;

    /* Intersect (x,y,w,h) with (cx,cy,cw,ch) */
    int nx = x > cx ? x : cx;
    int ny = y > cy ? y : cy;
    int nw = (x + w < cx + cw ? x + w : cx + cw) - nx;
    int nh = (y + h < cy + ch ? y + h : cy + ch) - ny;

    if (nw <= 0 || nh <= 0) return;
    
    for (int p_y = ny; p_y < ny + nh; p_y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)canvas->pixels + p_y * canvas->stride);
        for (int p_x = nx; p_x < nx + nw; p_x++) {
            row[p_x] = color;
        }
    }
}

lgui_canvas_t *lgui_canvas_create(void *pixels, uint32_t width, uint32_t height, uint32_t stride) {
    lgui_canvas_t *c = malloc(sizeof(lgui_canvas_t));
    if (c) {
        c->pixels = pixels;
        c->width = width;
        c->height = height;
        c->stride = stride;
        c->clip_count = 0;
        lgui_canvas_push_clip(c, 0, 0, width, height); /* Base clip */
    }
    return c;
}

void lgui_canvas_destroy(lgui_canvas_t *canvas) {
    free(canvas);
}

void lgui_canvas_draw_rect_outline(lgui_canvas_t *canvas,
                                    int x, int y, int w, int h, uint32_t color) {
    if (!canvas || w <= 0 || h <= 0) return;
    /* Top edge */
    lgui_canvas_fill_rect(canvas, x, y, w, 1, color);
    /* Bottom edge */
    lgui_canvas_fill_rect(canvas, x, y + h - 1, w, 1, color);
    /* Left edge */
    lgui_canvas_fill_rect(canvas, x, y, 1, h, color);
    /* Right edge */
    lgui_canvas_fill_rect(canvas, x + w - 1, y, 1, h, color);
}

void lgui_canvas_draw_text(lgui_canvas_t *canvas, int x, int y, const char *text, uint32_t color) {
    if (!canvas || !canvas->pixels) return;
    lgui_font_draw_text(canvas, x, y, text, color);
}
