/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef LUNAGUI_PRIVATE_H
#define LUNAGUI_PRIVATE_H

#include "lunagui.h"
#include "window_private.h"
#include <stdbool.h>

struct lgui_application_t {
    char name[64];
    int  lgp_fd;
    bool running;

    /* Capabilities granted by compositor during HELLO handshake */
    uint32_t caps_granted;

    /* Current pointer position (updated by POINTER_MOTION events) */
    int cursor_x;
    int cursor_y;

    struct lgui_window_t *windows[16];
    int window_count;

    struct lgui_widget_t *focused_widget;

    lgui_clipboard_cb clipboard_cb;
    void *clipboard_user_data;

    lgui_wm_surface_created_cb wm_surface_created_cb;
    lgui_wm_surface_destroyed_cb wm_surface_destroyed_cb;
    void *wm_user_data;

    lgui_global_key_cb global_key_cb;
    void *global_key_user_data;

    lgui_global_pointer_cb global_pointer_cb;
    void *global_pointer_user_data;

    uint32_t output_width;
    uint32_t output_height;
    lgui_output_geometry_cb output_geometry_cb;
    void *output_geometry_user_data;

    struct {
        int              fd;
        lgui_fd_callback_t cb;
        void            *user_data;
    } custom_fds[8];
    int custom_fd_count;
};

#define LGUI_MAX_CLIP_STACK 16

typedef struct {
    int x, y, w, h;
} lgui_clip_rect_t;

struct lgui_canvas_t {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;

    lgui_clip_rect_t clip_stack[LGUI_MAX_CLIP_STACK];
    int clip_count;
};

/* Render a widget tree into a canvas at (offset_x, offset_y). */
void lgui_widget_render(lgui_canvas_t *canvas, lgui_widget_t *w, int offset_x, int offset_y);

/* font.c — must be called once before any text rendering. */
void lgui_font_init(const char *psf_path);
void lgui_font_draw_text(lgui_canvas_t *canvas, int x, int y, const char *text, uint32_t color);

#endif
