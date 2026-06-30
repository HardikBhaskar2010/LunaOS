/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "keyboard.h"
#include "../logging/log.h"
#include "../protocol/tlv.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>

static int g_keyboard_fd = -1;
static uint32_t g_modifiers = 0;

/* Basic modifier masks for LGP */
#define LGP_MOD_SHIFT 0x01
#define LGP_MOD_CTRL  0x02
#define LGP_MOD_ALT   0x04
#define LGP_MOD_SUPER 0x08

static void update_modifiers(uint16_t code, int32_t value) {
    bool pressed = (value == 1 || value == 2);
    uint32_t mask = 0;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) mask = LGP_MOD_SHIFT;
    else if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) mask = LGP_MOD_CTRL;
    else if (code == KEY_LEFTALT || code == KEY_RIGHTALT) mask = LGP_MOD_ALT;
    else if (code == KEY_LEFTMETA || code == KEY_RIGHTMETA) mask = LGP_MOD_SUPER;

    if (mask != 0) {
        if (pressed) {
            g_modifiers |= mask;
        } else {
            g_modifiers &= ~mask;
        }
    }
}

void lgp_keyboard_init(void) {
    /* For simplicity in the v0.1 prototype, we'll try common event device paths
     * and pick the first one that supports KEY_A (indicating a keyboard). */
    char path[32];
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            unsigned long key_bitmask[128] = {0};
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bitmask)), key_bitmask) >= 0) {
                if (key_bitmask[KEY_A / (sizeof(long) * 8)] & (1UL << (KEY_A % (sizeof(long) * 8)))) {
                    g_keyboard_fd = fd;
                    LGP_INFO("input", "Keyboard initialized at %s", path);
                    return;
                }
            }
            close(fd);
        }
    }
    LGP_WARN("input", "Could not find a keyboard evdev device");
}

void lgp_keyboard_pump(lgp_compositor_state_t *state) {
    if (g_keyboard_fd < 0 || !state) return;

    struct input_event ev;
    while (read(g_keyboard_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_KEY) {
            update_modifiers(ev.code, ev.value);

            /* Route LGP_MSG_KEYBOARD_KEY to focused surface */
            if (state->keyboard_focus_session_id > 0) {
                lgp_dispatch_keyboard_key(state, ev.code, (uint32_t)ev.value, g_modifiers);
            }
        }
    }
}

int lgp_keyboard_get_fd(void) {
    return g_keyboard_fd;
}
