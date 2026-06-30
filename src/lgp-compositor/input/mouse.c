/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * mouse.c — Raw PS/2 mouse input from /dev/input/mice.
 *
 * Reads 3-byte PS/2 relative events, updates the cursor position (clamped to
 * the actual DRM display dimensions), and dispatches LGP_MSG_POINTER_MOTION
 * events to the surface under the cursor.
 *
 * The screen dimensions are set once at init time from the live DRM mode.
 */

#include "mouse.h"
#include "../logging/log.h"
#include "../protocol/tlv.h"
#include "../scene/surface.h"
#include <fcntl.h>
#include <unistd.h>

static int g_mouse_fd  = -1;
static int g_mouse_x   = 0;
static int g_mouse_y   = 0;
static int g_screen_w  = 1920; /* Updated at init from DRM mode */
static int g_screen_h  = 1080;

void lgp_mouse_init(uint32_t screen_width, uint32_t screen_height) {
    g_screen_w = (int)screen_width;
    g_screen_h = (int)screen_height;
    g_mouse_x  = g_screen_w / 2;
    g_mouse_y  = g_screen_h / 2;

    g_mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (g_mouse_fd < 0) {
        LGP_WARN("input", "Could not open /dev/input/mice — mouse input unavailable");
    } else {
        LGP_INFO("input", "Mouse input initialized (%dx%d screen space)",
                 g_screen_w, g_screen_h);
    }
}

void lgp_mouse_pump(lgp_compositor_state_t *state) {
    if (g_mouse_fd < 0) return;

    uint8_t data[3];
    while (read(g_mouse_fd, data, sizeof(data)) == (ssize_t)sizeof(data)) {
        /* PS/2 sign-extended relative deltas */
        int dx = (int)data[1];
        int dy = (int)data[2];

        if (data[0] & 0x10u) dx -= 256;   /* X sign bit */
        if (data[0] & 0x20u) dy -= 256;   /* Y sign bit */

        /* /dev/input/mice Y axis is inverted relative to screen coordinates */
        g_mouse_x += dx;
        g_mouse_y -= dy;

        /* Clamp to actual display dimensions */
        if (g_mouse_x < 0)            g_mouse_x = 0;
        if (g_mouse_x >= g_screen_w)  g_mouse_x = g_screen_w - 1;
        if (g_mouse_y < 0)            g_mouse_y = 0;
        if (g_mouse_y >= g_screen_h)  g_mouse_y = g_screen_h - 1;

        /* Dispatch motion event to the surface under the cursor */
        lgp_dispatch_pointer_motion(state, g_mouse_x, g_mouse_y);

        /* Handle LGP_MSG_POINTER_BUTTON for left/right clicks */
        static bool last_left = false;
        static bool last_right = false;

        bool left_button = (data[0] & 0x01u) != 0;
        bool right_button = (data[0] & 0x02u) != 0;

        if (left_button != last_left) {
            lgp_dispatch_pointer_button(state, g_mouse_x, g_mouse_y, 0, left_button);
            last_left = left_button;
        }
        if (right_button != last_right) {
            lgp_dispatch_pointer_button(state, g_mouse_x, g_mouse_y, 1, right_button);
            last_right = right_button;
        }
    }
}

int lgp_mouse_get_fd(void) {
    return g_mouse_fd;
}
