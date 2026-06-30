/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * window_private.h — internal struct definition for lgui_window_t.
 * Only included by modules that need direct struct access (application.c).
 */

#ifndef LUNAGUI_WINDOW_PRIVATE_H
#define LUNAGUI_WINDOW_PRIVATE_H

#include "lunagui.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct lgui_window_t {
    lgui_application_t *app;
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t layer;
    lgui_widget_t *root_widget;
    bool dirty; /* true when the widget tree has changed and needs re-render */
    int    buffer_fd;
    void  *buffer_map;
    size_t buffer_size;
    lgui_canvas_t *canvas;
};

#endif
