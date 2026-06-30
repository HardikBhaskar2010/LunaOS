/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * widget.c — Common widget operations shared across all widget types.
 *
 * lgui_widget_destroy() recursively frees a widget tree.
 * lgui_widget_set_size() sets explicit dimensions to override the layout engine.
 */

#include "lunagui.h"
#include <stdlib.h>

#include "widget_private.h"

void lgui_widget_set_size(lgui_widget_t *widget, int width, int height) {
    if (!widget) return;
    widget->width  = width;
    widget->height = height;
}

void lgui_widget_set_on_key(lgui_widget_t *widget, lgui_key_cb cb, void *user_data) {
    if (!widget) return;
    widget->on_key = cb;
    widget->user_data = user_data;
}

/*
 * lgui_widget_destroy() — Recursively free a widget and all of its children.
 *
 * After calling this, any pointer to this widget or its descendants is invalid.
 * Do NOT call this on a widget still attached to a visible window.
 */
void lgui_widget_destroy(lgui_widget_t *widget) {
    if (!widget) return;

    /* Recursively free all children first */
    for (int i = 0; i < widget->child_count; i++) {
        lgui_widget_destroy(widget->children[i]);
        widget->children[i] = NULL;
    }

    free(widget);
}
