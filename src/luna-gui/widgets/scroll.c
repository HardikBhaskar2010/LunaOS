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

lgui_widget_t *lgui_scroll_container_create(void) {
    lgui_widget_t *scroll = calloc(1, sizeof(lgui_widget_t));
    if (scroll) {
        scroll->type = 5; /* Scroll */
    }
    return scroll;
}

void lgui_scroll_container_set_child(lgui_widget_t *scroll, lgui_widget_t *child) {
    if (!scroll || scroll->type != 5 || !child) return;
    
    if (scroll->child_count > 0) {
        lgui_widget_destroy(scroll->children[0]);
    }
    scroll->children[0] = child;
    scroll->child_count = 1;
}
