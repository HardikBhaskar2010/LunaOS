/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "lunagui.h"
#include <stdlib.h>
#include <string.h>

#include "widget_private.h"

lgui_widget_t *lgui_canvas_widget_create(void) {
    lgui_widget_t *w = calloc(1, sizeof(lgui_widget_t));
    if (w) {
        w->type = 6; /* Canvas Widget */
    }
    return w;
}

void lgui_canvas_widget_set_render(lgui_widget_t *widget, lgui_canvas_render_cb cb) {
    if (!widget || widget->type != 6) return;
    widget->canvas_render = cb;
}
