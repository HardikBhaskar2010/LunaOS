/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * LunaGUI Application — LGP client connection and main event loop.
 *
 * This module opens a connection to the lgp-compositor socket, performs the
 * LGP_HELLO handshake (per DL-053 wire format), and drives the poll()-based
 * event loop that dispatches both compositor input events and user-registered
 * file descriptors.
 */

#include "lunagui.h"
#include "lgui_private.h"
#include "window_private.h"
#include "widget_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "lgui_private.h"

/* LGP wire-format constants — must match lgp-compositor/protocol/ */
#define LGP_SOCKET_PATH        "/run/lgp/compositor.sock"
#define LGP_HEADER_SIZE        6u
#define LGP_MSG_HELLO          0x0001u
#define LGP_MSG_HELLO_REPLY    0x0002u
#define LGP_VERSION_MAJOR      1u
#define LGP_VERSION_MINOR      0u
/* Capability bits (must match lgp-compositor/protocol/caps.h) */
#define LGP_CAP_CANVAS_SURFACE (1u << 1)
#define LGP_CAP_LAYER_SHELL    (1u << 3)
#define LGP_CAP_LUNA_ISLAND    (1u << 4)
#define LGP_CAP_CLIPBOARD      (1u << 6)
#define LGP_CAP_WINDOW_MANAGER (1u << 7)

#define LGP_MSG_WM_SURFACE_CREATED   0x0200u
#define LGP_MSG_WM_SURFACE_DESTROYED 0x0201u
#define LGP_MSG_WM_SET_SURFACE_POSITION 0x0202u
#define LGP_MSG_WM_SET_FOCUS         0x0203u
#define LGP_MSG_WM_SET_STATE         0x0204u
#define LGP_MSG_WM_GRAB_KEY          0x0205u

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

static void write_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Write all bytes to fd, retrying on EINTR. Returns false on permanent error. */
static bool lgui_write_all(int fd, const uint8_t *buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        written += (size_t)n;
    }
    return true;
}

/* Read exactly `len` bytes from fd, retrying on EINTR.
 * Returns false if the connection closes or a permanent error occurs. */
static bool lgui_read_exact(int fd, uint8_t *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, buf + got, len - got);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false; /* EOF — compositor disconnected */
        got += (size_t)n;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * LGP HELLO handshake
 *
 * Wire format for LGP_HELLO (per DL-053 / hello.h):
 *   [6-byte TLV header: uint16_t type | uint32_t total_length]
 *   uint16_t version_major  (LE)
 *   uint16_t version_minor  (LE)
 *   uint32_t caps_requested (LE)
 *
 * The compositor replies with LGP_HELLO_REPLY (same structure, caps = granted).
 * We must read and parse the reply before sending any other messages.
 * ---------------------------------------------------------------------------*/
static bool lgui_do_hello(int fd, uint32_t caps_requested, uint32_t *out_caps_granted) {
    /* HELLO payload: 2 (major) + 2 (minor) + 4 (caps) = 8 bytes */
    const uint32_t payload_len = 8u;
    const uint32_t total_len   = (uint32_t)LGP_HEADER_SIZE + payload_len;

    uint8_t hello[LGP_HEADER_SIZE + 8u];
    write_u16_le(hello + 0, (uint16_t)LGP_MSG_HELLO);
    write_u32_le(hello + 2, total_len);
    write_u16_le(hello + 6, (uint16_t)LGP_VERSION_MAJOR);
    write_u16_le(hello + 8, (uint16_t)LGP_VERSION_MINOR);
    write_u32_le(hello + 10, caps_requested);

    if (!lgui_write_all(fd, hello, sizeof(hello))) {
        fprintf(stderr, "lunagui: failed to send LGP_HELLO\n");
        return false;
    }

    /* Read the 6-byte TLV header of the reply */
    uint8_t reply_hdr[LGP_HEADER_SIZE];
    if (!lgui_read_exact(fd, reply_hdr, sizeof(reply_hdr))) {
        fprintf(stderr, "lunagui: failed to read LGP_HELLO_REPLY header\n");
        return false;
    }

    uint16_t reply_type = (uint16_t)(reply_hdr[0] | (reply_hdr[1] << 8));
    uint32_t reply_len  = read_u32_le(reply_hdr + 2);

    if (reply_type != LGP_MSG_HELLO_REPLY) {
        fprintf(stderr, "lunagui: expected HELLO_REPLY (0x%04X), got 0x%04X\n",
                LGP_MSG_HELLO_REPLY, reply_type);
        return false;
    }

    if (reply_len < LGP_HEADER_SIZE + 8u) {
        fprintf(stderr, "lunagui: HELLO_REPLY payload too short (%u bytes)\n", reply_len);
        return false;
    }

    /* Read the HELLO_REPLY payload */
    const uint32_t payload_bytes = reply_len - (uint32_t)LGP_HEADER_SIZE;
    uint8_t *reply_payload = malloc(payload_bytes);
    if (!reply_payload) return false;

    bool ok = lgui_read_exact(fd, reply_payload, payload_bytes);
    if (ok && out_caps_granted) {
        /* Payload: uint16_t major, uint16_t minor, uint32_t caps_granted */
        *out_caps_granted = read_u32_le(reply_payload + 4);
    }
    free(reply_payload);

    if (!ok) {
        fprintf(stderr, "lunagui: failed to read LGP_HELLO_REPLY payload\n");
        return false;
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

lgui_application_t *lgui_application_create(const char *name) {
    if (!name) return NULL;

    lgui_application_t *app = calloc(1, sizeof(lgui_application_t));
    if (!app) return NULL;

    strncpy(app->name, name, sizeof(app->name) - 1);
    app->lgp_fd = -1;

    /* Initialise font rendering (loads PSF font if available) */
    lgui_font_init("/usr/share/fonts/luna-8x16.psf");

    /* Open connection to the compositor */
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        free(app);
        return NULL;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LGP_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "lunagui: cannot connect to compositor at %s: %s\n",
                LGP_SOCKET_PATH, strerror(errno));
        close(fd);
        free(app);
        return NULL;
    }

    app->lgp_fd = fd;

    /* Perform the LGP_HELLO handshake — mandatory before any other message */
    uint32_t caps_granted = 0;
    uint32_t caps_req = LGP_CAP_CANVAS_SURFACE | LGP_CAP_LAYER_SHELL | LGP_CAP_LUNA_ISLAND | LGP_CAP_CLIPBOARD;
    if (!lgui_do_hello(fd, caps_req, &caps_granted)) {
        close(fd);
        free(app);
        return NULL;
    }
    app->caps_granted = caps_granted;

    /* Immediately read the LGP_MSG_OUTPUT_GEOMETRY message */
    uint8_t geom_hdr[LGP_HEADER_SIZE];
    if (lgui_read_exact(fd, geom_hdr, sizeof(geom_hdr))) {
        uint16_t type = (uint16_t)(geom_hdr[0] | (geom_hdr[1] << 8));
        uint32_t len  = read_u32_le(geom_hdr + 2);
        if (type == 0x0300u && len >= LGP_HEADER_SIZE + 8u) {
            uint8_t payload[8];
            if (lgui_read_exact(fd, payload, 8)) {
                app->output_width = read_u32_le(payload + 0);
                app->output_height = read_u32_le(payload + 4);
            }
        }
    }

    return app;
}

lgui_application_t *lgui_application_create_wm(const char *name, 
                                               lgui_wm_surface_created_cb on_created, 
                                               lgui_wm_surface_destroyed_cb on_destroyed,
                                               void *user_data) {
    if (!name) return NULL;

    lgui_application_t *app = calloc(1, sizeof(lgui_application_t));
    if (!app) return NULL;

    strncpy(app->name, name, sizeof(app->name) - 1);
    app->lgp_fd = -1;
    app->wm_surface_created_cb = on_created;
    app->wm_surface_destroyed_cb = on_destroyed;
    app->wm_user_data = user_data;

    lgui_font_init("/usr/share/fonts/luna-8x16.psf");

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        free(app);
        return NULL;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LGP_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        free(app);
        return NULL;
    }

    app->lgp_fd = fd;

    uint32_t caps_granted = 0;
    uint32_t caps_req = LGP_CAP_CANVAS_SURFACE | LGP_CAP_LAYER_SHELL | LGP_CAP_LUNA_ISLAND | LGP_CAP_WINDOW_MANAGER;
    if (!lgui_do_hello(fd, caps_req, &caps_granted)) {
        close(fd);
        free(app);
        return NULL;
    }
    app->caps_granted = caps_granted;

    /* Immediately read the LGP_MSG_OUTPUT_GEOMETRY message */
    uint8_t geom_hdr[LGP_HEADER_SIZE];
    if (lgui_read_exact(fd, geom_hdr, sizeof(geom_hdr))) {
        uint16_t type = (uint16_t)(geom_hdr[0] | (geom_hdr[1] << 8));
        uint32_t len  = read_u32_le(geom_hdr + 2);
        if (type == 0x0300u && len >= LGP_HEADER_SIZE + 8u) {
            uint8_t payload[8];
            if (lgui_read_exact(fd, payload, 8)) {
                app->output_width = read_u32_le(payload + 0);
                app->output_height = read_u32_le(payload + 4);
            }
        }
    }

    return app;
}

void lgui_application_destroy(lgui_application_t *app) {
    if (!app) return;
    if (app->lgp_fd >= 0) close(app->lgp_fd);
    free(app);
}

void lgui_application_quit(lgui_application_t *app) {
    if (app) app->running = false;
}

void lgui_application_add_fd(lgui_application_t *app, int fd, lgui_fd_callback_t cb, void *user_data) {
    if (!app || app->custom_fd_count >= 8) return;
    app->custom_fds[app->custom_fd_count].fd        = fd;
    app->custom_fds[app->custom_fd_count].cb        = cb;
    app->custom_fds[app->custom_fd_count].user_data = user_data;
    app->custom_fd_count++;
}

void lgui_clipboard_set_text(lgui_application_t *app, const char *text) {
    if (!app || app->lgp_fd < 0 || !text) return;
    
    size_t text_len = strlen(text);
    uint32_t total_len = LGP_HEADER_SIZE + (uint32_t)text_len;
    
    uint8_t *msg = malloc(total_len);
    if (!msg) return;
    
    write_u16_le(msg + 0, 0x0120u); /* LGP_MSG_CLIPBOARD_SET */
    write_u32_le(msg + 2, total_len);
    memcpy(msg + LGP_HEADER_SIZE, text, text_len);
    
    lgui_write_all(app->lgp_fd, msg, total_len);
    free(msg);
}

void lgui_clipboard_request_text(lgui_application_t *app, lgui_clipboard_cb cb, void *user_data) {
    if (!app || app->lgp_fd < 0 || !cb) return;
    
    app->clipboard_cb = cb;
    app->clipboard_user_data = user_data;
    
    uint8_t msg[LGP_HEADER_SIZE];
    write_u16_le(msg + 0, 0x0121u); /* LGP_MSG_CLIPBOARD_GET */
    write_u32_le(msg + 2, LGP_HEADER_SIZE);
    
    lgui_write_all(app->lgp_fd, msg, sizeof(msg));
}

/* ---------------------------------------------------------------------------
 * Widget hit-testing and input routing
 * ---------------------------------------------------------------------------*/

/*
 * lgui_widget_hittest() — Find the deepest widget under screen-space (x,y)
 * within the widget tree rooted at `w`. `ox/oy` is the accumulated offset
 * for this subtree. Returns the matching widget or NULL.
 */
static lgui_widget_t *lgui_widget_hittest(lgui_widget_t *w,
                                           int ox, int oy,
                                           int x, int y) {
    if (!w) return NULL;

    int wx = ox + w->x;
    int wy = oy + w->y;

    /* Check children first (front-to-back order) */
    for (int i = w->child_count - 1; i >= 0; i--) {
        lgui_widget_t *hit = lgui_widget_hittest(w->children[i], wx, wy, x, y);
        if (hit) return hit;
    }

    /* Check this widget */
    if (x >= wx && x < wx + w->width &&
        y >= wy && y < wy + w->height) {
        return w;
    }

    return NULL;
}

static void lgui_dispatch_pointer_button(lgui_application_t *app,
                                          int x, int y, bool pressed) {
    if (!pressed) return; /* Only fire on button-down */

    for (int i = 0; i < app->window_count; i++) {
        lgui_window_t *win = app->windows[i];
        if (!win || !win->root_widget) continue;

        lgui_widget_t *hit = lgui_widget_hittest(win->root_widget, 0, 0, x, y);
        if (hit) {
            app->focused_widget = hit;
            if (hit->type == 2 /* Button */ && hit->on_click) {
                hit->on_click(hit, hit->user_data);
                /* Window tree may have changed; mark all windows dirty */
                for (int j = 0; j < app->window_count; j++) {
                    if (app->windows[j]) app->windows[j]->dirty = true;
                }
            }
            return;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Window Management (WM) APIs
 * ---------------------------------------------------------------------------*/

void lgui_wm_set_surface_position(lgui_application_t *app, uint32_t surface_id, int x, int y) {
    if (!app || app->lgp_fd < 0) return;
    uint8_t buf[LGP_HEADER_SIZE + 12];
    write_u16_le(buf, LGP_MSG_WM_SET_SURFACE_POSITION);
    write_u32_le(buf + 2, sizeof(buf));
    write_u32_le(buf + LGP_HEADER_SIZE, surface_id);
    write_u32_le(buf + LGP_HEADER_SIZE + 4, (uint32_t)x);
    write_u32_le(buf + LGP_HEADER_SIZE + 8, (uint32_t)y);
    lgui_write_all(app->lgp_fd, buf, sizeof(buf));
}

void lgui_wm_set_focus(lgui_application_t *app, uint32_t surface_id) {
    if (!app || app->lgp_fd < 0) return;
    uint8_t buf[LGP_HEADER_SIZE + 4];
    write_u16_le(buf, LGP_MSG_WM_SET_FOCUS);
    write_u32_le(buf + 2, sizeof(buf));
    write_u32_le(buf + LGP_HEADER_SIZE, surface_id);
    lgui_write_all(app->lgp_fd, buf, sizeof(buf));
}

uint32_t lgui_output_width(lgui_application_t *app) {
    return app ? app->output_width : 0;
}

uint32_t lgui_output_height(lgui_application_t *app) {
    return app ? app->output_height : 0;
}

void lgui_application_set_output_geometry_cb(lgui_application_t *app, lgui_output_geometry_cb cb, void *user_data) {
    if (app) {
        app->output_geometry_cb = cb;
        app->output_geometry_user_data = user_data;
    }
}

void lgui_wm_set_state(lgui_application_t *app, uint32_t surface_id, uint32_t state) {
    if (!app || app->lgp_fd < 0) return;
    uint8_t buf[LGP_HEADER_SIZE + 8];
    write_u16_le(buf, LGP_MSG_WM_SET_STATE);
    write_u32_le(buf + 2, sizeof(buf));
    write_u32_le(buf + LGP_HEADER_SIZE, surface_id);
    write_u32_le(buf + LGP_HEADER_SIZE + 4, state);
    lgui_write_all(app->lgp_fd, buf, sizeof(buf));
}

void lgui_wm_grab_key(lgui_application_t *app, uint32_t key, uint32_t modifiers) {
    if (!app || app->lgp_fd < 0) return;
    uint8_t buf[LGP_HEADER_SIZE + 8];
    write_u16_le(buf, LGP_MSG_WM_GRAB_KEY);
    write_u32_le(buf + 2, sizeof(buf));
    write_u32_le(buf + LGP_HEADER_SIZE, key);
    write_u32_le(buf + LGP_HEADER_SIZE + 4, modifiers);
    lgui_write_all(app->lgp_fd, buf, sizeof(buf));
}

void lgui_application_set_global_key_cb(lgui_application_t *app, lgui_global_key_cb cb, void *user_data) {
    if (app) {
        app->global_key_cb = cb;
        app->global_key_user_data = user_data;
    }
}

void lgui_application_set_global_pointer_cb(lgui_application_t *app, lgui_global_pointer_cb cb, void *user_data) {
    if (app) {
        app->global_pointer_cb = cb;
        app->global_pointer_user_data = user_data;
    }
}

void lgui_widget_focus(lgui_application_t *app, lgui_widget_t *widget) {
    if (app) app->focused_widget = widget;
}

int lgui_application_run(lgui_application_t *app) {
    if (!app) return -1;
    app->running = true;

    /* +1 for the compositor fd, +N for user-registered fds */
    struct pollfd pfds[9];

    while (app->running) {
        pfds[0].fd      = app->lgp_fd;
        pfds[0].events  = POLLIN;
        pfds[0].revents = 0;

        int pfd_count = 1;
        for (int i = 0; i < app->custom_fd_count; i++) {
            pfds[pfd_count].fd      = app->custom_fds[i].fd;
            pfds[pfd_count].events  = POLLIN;
            pfds[pfd_count].revents = 0;
            pfd_count++;
        }

        int r = poll(pfds, (nfds_t)pfd_count, 16); /* ~60 fps poll interval */
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Compositor fd events */
        if (pfds[0].revents & (POLLERR | POLLHUP)) {
            break; /* Compositor disconnected */
        }

        if (pfds[0].revents & POLLIN) {
            uint8_t rx[4096];
            ssize_t n = read(app->lgp_fd, rx, sizeof(rx));
            if (n <= 0) break;

            /* Parse incoming TLV frames */
            size_t offset = 0;
            while (offset + (size_t)LGP_HEADER_SIZE <= (size_t)n) {
                uint16_t type = (uint16_t)(rx[offset] | (rx[offset + 1] << 8));
                uint32_t len  = read_u32_le(rx + offset + 2);

                if (len < LGP_HEADER_SIZE || offset + len > (size_t)n) break;

                const uint8_t *payload = rx + offset + LGP_HEADER_SIZE;
                uint32_t payload_len = len - (uint32_t)LGP_HEADER_SIZE;

                if (type == 0x0110u) {
                    /* LGP_MSG_POINTER_MOTION: int32_t x, int32_t y, surface-local for apps.
                     * The WM receives the same message in global output coordinates. */
                    if (payload_len >= 8u) {
                        int px = (int)read_u32_le(payload + 0);
                        int py = (int)read_u32_le(payload + 4);
                        app->cursor_x = px;
                        app->cursor_y = py;

                        if (app->global_pointer_cb) {
                            app->global_pointer_cb(px, py, false, false, app->global_pointer_user_data);
                        }
                    }
                } else if (type == 0x0111u) {
                    /* LGP_MSG_POINTER_BUTTON: uint8_t button, uint8_t pressed */
                    if (payload_len >= 2u) {
                        bool pressed = payload[1] != 0u;
                        lgui_dispatch_pointer_button(app,
                                                      app->cursor_x, app->cursor_y,
                                                      pressed);
                        if (app->global_pointer_cb) {
                            app->global_pointer_cb(app->cursor_x, app->cursor_y, pressed, true, app->global_pointer_user_data);
                        }
                    }
                } else if (type == 0x0112u) {
                    /* LGP_MSG_KEYBOARD_KEY: uint32 key, uint32 state, uint32 modifiers */
                    if (payload_len >= 12u) {
                        uint32_t key = read_u32_le(payload + 0);
                        uint32_t state = read_u32_le(payload + 4);
                        uint32_t modifiers = read_u32_le(payload + 8);
                        
                        /* Dispatch to global callback if registered (e.g. for WM) */
                        if (state != 0 && app->global_key_cb) {
                            app->global_key_cb(key, modifiers, app->global_key_user_data);
                        }
                        
                        /* Also dispatch to focused widget */
                        if (state != 0 && app->focused_widget && app->focused_widget->on_key) {
                            app->focused_widget->on_key(app->focused_widget, key, modifiers, app->focused_widget->user_data);
                        }
                    }
                } else if (type == 0x0122u) {
                    /* LGP_MSG_CLIPBOARD_DATA: string payload */
                    if (app->clipboard_cb) {
                        char *text = NULL;
                        if (payload_len > 0) {
                            text = malloc(payload_len + 1);
                            if (text) {
                                memcpy(text, payload, payload_len);
                                text[payload_len] = '\0';
                            }
                        }
                        app->clipboard_cb(text, app->clipboard_user_data);
                        if (text) free(text);
                        app->clipboard_cb = NULL;
                    }
                } else if (type == LGP_MSG_WM_SURFACE_CREATED) {
                    if (payload_len >= 16u && app->wm_surface_created_cb) {
                        uint32_t surface_id = read_u32_le(payload + 0);
                        uint32_t surface_type = read_u32_le(payload + 4);
                        uint32_t w = read_u32_le(payload + 8);
                        uint32_t h = read_u32_le(payload + 12);
                        app->wm_surface_created_cb(surface_id, surface_type, w, h, app->wm_user_data);
                    }
                } else if (type == LGP_MSG_WM_SURFACE_DESTROYED) {
                    if (payload_len >= 4u && app->wm_surface_destroyed_cb) {
                        uint32_t surface_id = read_u32_le(payload + 0);
                        app->wm_surface_destroyed_cb(surface_id, app->wm_user_data);
                    }
                } else if (type == 0x0300u) { /* LGP_MSG_OUTPUT_GEOMETRY */
                    if (payload_len >= 8u) {
                        uint32_t w = read_u32_le(payload + 0);
                        uint32_t h = read_u32_le(payload + 4);
                        app->output_width = w;
                        app->output_height = h;
                        if (app->output_geometry_cb) {
                            app->output_geometry_cb(w, h, app->output_geometry_user_data);
                        }
                    }
                }
                offset += len;
            }
        }

        /* User-registered fd callbacks */
        for (int i = 0; i < app->custom_fd_count; i++) {
            if (pfds[i + 1].revents & POLLIN) {
                app->custom_fds[i].cb(app->custom_fds[i].fd, app->custom_fds[i].user_data);
            }
        }

        /* Re-render any dirty windows */
        for (int i = 0; i < app->window_count; i++) {
            if (app->windows[i] && app->windows[i]->dirty) {
                lgui_window_update(app->windows[i]);
            }
        }
    }

    return 0;
}
