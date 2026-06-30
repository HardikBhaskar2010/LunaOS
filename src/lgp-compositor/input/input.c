/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "input.h"
#include "../logging/log.h"
#include "../scene/surface.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Basic modifier masks for LGP clients. */
#define LGP_MOD_SHIFT 0x01u
#define LGP_MOD_CTRL  0x02u
#define LGP_MOD_ALT   0x04u
#define LGP_MOD_SUPER 0x08u

typedef struct {
    int      screen_w;
    int      screen_h;
    double   pointer_x;
    double   pointer_y;
    uint32_t modifiers;
} lgp_input_shared_t;

static lgp_input_shared_t g_input = {
    .screen_w = 1920,
    .screen_h = 1080,
    .pointer_x = 960.0,
    .pointer_y = 540.0,
    .modifiers = 0u
};

static void lgp_input_set_screen(uint32_t screen_width, uint32_t screen_height) {
    g_input.screen_w = screen_width > 0u ? (int)screen_width : 1920;
    g_input.screen_h = screen_height > 0u ? (int)screen_height : 1080;
    g_input.pointer_x = (double)(g_input.screen_w / 2);
    g_input.pointer_y = (double)(g_input.screen_h / 2);
    g_input.modifiers = 0u;

    lgp_cursor_init((uint32_t)g_input.screen_w, (uint32_t)g_input.screen_h);
    lgp_cursor_set_position((int32_t)g_input.pointer_x, (int32_t)g_input.pointer_y);
}

static int lgp_input_pointer_x(void) {
    return (int)g_input.pointer_x;
}

static int lgp_input_pointer_y(void) {
    return (int)g_input.pointer_y;
}

static void lgp_input_clamp_pointer(void) {
    if (g_input.pointer_x < 0.0) {
        g_input.pointer_x = 0.0;
    }
    if (g_input.pointer_y < 0.0) {
        g_input.pointer_y = 0.0;
    }
    if (g_input.pointer_x >= (double)g_input.screen_w) {
        g_input.pointer_x = (double)(g_input.screen_w - 1);
    }
    if (g_input.pointer_y >= (double)g_input.screen_h) {
        g_input.pointer_y = (double)(g_input.screen_h - 1);
    }
}

static void lgp_input_update_modifiers(uint32_t code, bool pressed) {
    uint32_t mask = 0u;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        mask = LGP_MOD_SHIFT;
    } else if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) {
        mask = LGP_MOD_CTRL;
    } else if (code == KEY_LEFTALT || code == KEY_RIGHTALT) {
        mask = LGP_MOD_ALT;
    } else if (code == KEY_LEFTMETA || code == KEY_RIGHTMETA) {
        mask = LGP_MOD_SUPER;
    }

    if (mask == 0u) {
        return;
    }

    if (pressed) {
        g_input.modifiers |= mask;
    } else {
        g_input.modifiers &= ~mask;
    }
}

static uint8_t lgp_input_button_from_evdev(uint32_t button) {
    if (button == BTN_LEFT) {
        return 0u;
    }
    if (button == BTN_RIGHT) {
        return 1u;
    }
    if (button == BTN_MIDDLE) {
        return 2u;
    }
    return 3u;
}

#ifdef LGP_HAVE_LIBINPUT

#include <libinput.h>
#include <libudev.h>

static struct libinput *g_libinput = NULL;
static struct udev     *g_udev = NULL;

static int lgp_libinput_open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;

    int fd = open(path, flags | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }
    return fd;
}

static void lgp_libinput_close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface g_libinput_interface = {
    .open_restricted = lgp_libinput_open_restricted,
    .close_restricted = lgp_libinput_close_restricted
};

static void lgp_input_handle_pointer_motion(lgp_compositor_state_t *state,
                                            struct libinput_event *event) {
    struct libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
    if (!pointer) {
        return;
    }

    g_input.pointer_x += libinput_event_pointer_get_dx(pointer);
    g_input.pointer_y += libinput_event_pointer_get_dy(pointer);
    lgp_input_clamp_pointer();

    int x = lgp_input_pointer_x();
    int y = lgp_input_pointer_y();
    lgp_cursor_set_position(x, y);
    lgp_dispatch_pointer_motion(state, x, y);
}

static void lgp_input_handle_pointer_button(lgp_compositor_state_t *state,
                                            struct libinput_event *event) {
    struct libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
    if (!pointer) {
        return;
    }

    uint32_t evdev_button = libinput_event_pointer_get_button(pointer);
    enum libinput_button_state button_state =
        libinput_event_pointer_get_button_state(pointer);
    bool pressed = button_state == LIBINPUT_BUTTON_STATE_PRESSED;

    lgp_dispatch_pointer_button(state,
                                lgp_input_pointer_x(),
                                lgp_input_pointer_y(),
                                lgp_input_button_from_evdev(evdev_button),
                                pressed);
}

static void lgp_input_handle_keyboard_key(lgp_compositor_state_t *state,
                                          struct libinput_event *event) {
    struct libinput_event_keyboard *keyboard = libinput_event_get_keyboard_event(event);
    if (!keyboard) {
        return;
    }

    uint32_t key = libinput_event_keyboard_get_key(keyboard);
    enum libinput_key_state key_state = libinput_event_keyboard_get_key_state(keyboard);
    bool pressed = key_state == LIBINPUT_KEY_STATE_PRESSED;

    lgp_input_update_modifiers(key, pressed);
    lgp_dispatch_keyboard_key(state, key, pressed ? 1u : 0u, g_input.modifiers);
}

void lgp_input_init(uint32_t screen_width, uint32_t screen_height) {
    lgp_input_set_screen(screen_width, screen_height);

    g_udev = udev_new();
    if (!g_udev) {
        LGP_WARN("input", "udev_new() failed; input unavailable");
        return;
    }

    g_libinput = libinput_udev_create_context(&g_libinput_interface, NULL, g_udev);
    if (!g_libinput) {
        LGP_WARN("input", "libinput context creation failed; input unavailable");
        udev_unref(g_udev);
        g_udev = NULL;
        return;
    }

    if (libinput_udev_assign_seat(g_libinput, "seat0") != 0) {
        LGP_WARN("input", "libinput could not assign seat0; input unavailable");
        libinput_unref(g_libinput);
        udev_unref(g_udev);
        g_libinput = NULL;
        g_udev = NULL;
        return;
    }

    libinput_dispatch(g_libinput);
    LGP_INFO("input", "libinput backend initialized (%dx%d screen space)",
             g_input.screen_w, g_input.screen_h);
}

int lgp_input_get_fd(void) {
    if (!g_libinput) {
        return -1;
    }
    return libinput_get_fd(g_libinput);
}

int lgp_input_get_aux_fd(void) {
    return -1;
}

void lgp_input_pump(lgp_compositor_state_t *state) {
    if (!g_libinput || !state) {
        return;
    }

    libinput_dispatch(g_libinput);

    struct libinput_event *event = NULL;
    while ((event = libinput_get_event(g_libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);

        if (type == LIBINPUT_EVENT_POINTER_MOTION) {
            lgp_input_handle_pointer_motion(state, event);
        } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
            lgp_input_handle_pointer_button(state, event);
        } else if (type == LIBINPUT_EVENT_KEYBOARD_KEY) {
            lgp_input_handle_keyboard_key(state, event);
        }

        libinput_event_destroy(event);
        libinput_dispatch(g_libinput);
    }
}

void lgp_input_cleanup(void) {
    if (g_libinput) {
        libinput_unref(g_libinput);
        g_libinput = NULL;
    }
    if (g_udev) {
        udev_unref(g_udev);
        g_udev = NULL;
    }
}

#else

#include <linux/input.h>
#include <sys/ioctl.h>

static int g_mouse_fd = -1;
static int g_keyboard_fd = -1;

static void lgp_input_init_raw_mouse(void) {
    g_mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (g_mouse_fd < 0) {
        LGP_WARN("input", "Could not open /dev/input/mice; mouse input unavailable");
    } else {
        LGP_INFO("input", "Raw PS/2 mouse fallback initialized");
    }
}

static void lgp_input_init_raw_keyboard(void) {
    char path[32];

    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }

        unsigned long key_bitmask[128] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bitmask)), key_bitmask) >= 0) {
            unsigned long word = key_bitmask[KEY_A / (sizeof(unsigned long) * 8u)];
            unsigned long bit = 1UL << (KEY_A % (sizeof(unsigned long) * 8u));
            if ((word & bit) != 0UL) {
                g_keyboard_fd = fd;
                LGP_INFO("input", "Raw evdev keyboard fallback initialized at %s", path);
                return;
            }
        }
        close(fd);
    }

    LGP_WARN("input", "Could not find a keyboard evdev device");
}

void lgp_input_init(uint32_t screen_width, uint32_t screen_height) {
    lgp_input_set_screen(screen_width, screen_height);
    LGP_WARN("input", "libinput unavailable at build time; using raw fallback input");
    lgp_input_init_raw_mouse();
    lgp_input_init_raw_keyboard();
}

int lgp_input_get_fd(void) {
    if (g_mouse_fd >= 0) {
        return g_mouse_fd;
    }
    return g_keyboard_fd;
}

int lgp_input_get_aux_fd(void) {
    if (g_mouse_fd >= 0 && g_keyboard_fd >= 0) {
        return g_keyboard_fd;
    }
    return -1;
}

static void lgp_input_pump_raw_mouse(lgp_compositor_state_t *state) {
    if (g_mouse_fd < 0 || !state) {
        return;
    }

    uint8_t data[3];
    while (read(g_mouse_fd, data, sizeof(data)) == (ssize_t)sizeof(data)) {
        int dx = (int)data[1];
        int dy = (int)data[2];

        if ((data[0] & 0x10u) != 0u) {
            dx -= 256;
        }
        if ((data[0] & 0x20u) != 0u) {
            dy -= 256;
        }

        g_input.pointer_x += (double)dx;
        g_input.pointer_y -= (double)dy;
        lgp_input_clamp_pointer();

        int x = lgp_input_pointer_x();
        int y = lgp_input_pointer_y();
        lgp_cursor_set_position(x, y);
        lgp_dispatch_pointer_motion(state, x, y);

        static bool last_left = false;
        static bool last_right = false;

        bool left_button = (data[0] & 0x01u) != 0u;
        bool right_button = (data[0] & 0x02u) != 0u;

        if (left_button != last_left) {
            lgp_dispatch_pointer_button(state, x, y, 0u, left_button);
            last_left = left_button;
        }
        if (right_button != last_right) {
            lgp_dispatch_pointer_button(state, x, y, 1u, right_button);
            last_right = right_button;
        }
    }
}

static void lgp_input_pump_raw_keyboard(lgp_compositor_state_t *state) {
    if (g_keyboard_fd < 0 || !state) {
        return;
    }

    struct input_event ev;
    while (read(g_keyboard_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type != EV_KEY) {
            continue;
        }

        bool pressed = ev.value == 1 || ev.value == 2;
        lgp_input_update_modifiers(ev.code, pressed);

        if (state->keyboard_focus_session_id > 0) {
            lgp_dispatch_keyboard_key(state, ev.code, (uint32_t)ev.value, g_input.modifiers);
        }
    }
}

void lgp_input_pump(lgp_compositor_state_t *state) {
    lgp_input_pump_raw_mouse(state);
    lgp_input_pump_raw_keyboard(state);
}

void lgp_input_cleanup(void) {
    if (g_mouse_fd >= 0) {
        close(g_mouse_fd);
        g_mouse_fd = -1;
    }
    if (g_keyboard_fd >= 0) {
        close(g_keyboard_fd);
        g_keyboard_fd = -1;
    }
}

#endif
