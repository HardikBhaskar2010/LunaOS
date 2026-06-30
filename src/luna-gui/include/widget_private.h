/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef LUNAGUI_WIDGET_PRIVATE_H
#define LUNAGUI_WIDGET_PRIVATE_H

#include "lunagui.h"

struct lgui_widget_t {
    int type; /* 1=Label, 2=Button, 3=VBox, 4=HBox, 5=Scroll */
    int x, y, width, height;
    char text[256];
    
    int scroll_offset_x;
    int scroll_offset_y;

    lgui_button_click_cb  on_click;
    lgui_key_cb           on_key;
    lgui_canvas_render_cb canvas_render;
    void *user_data;
    
    lgui_widget_t *children[32];
    int child_count;
};

#endif
