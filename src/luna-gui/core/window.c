/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * LunaGUI Window — manages a shared-memory surface backed by a memfd,
 * communicates with lgp-compositor via LGP_CREATE_SURFACE / LGP_COMMIT_BUFFER.
 *
 * Wire format constants are defined locally and must stay in sync with
 * lgp-compositor/protocol/tlv.h.
 */

#include "lunagui.h"
#include "lgui_private.h"
#include "window_private.h"
#include "widget_private.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

/* LGP message type constants (must match lgp-compositor/protocol/tlv.h) */
#define LGP_HEADER_SIZE              6u
#define LGP_MSG_CREATE_SURFACE       0x0100u
#define LGP_MSG_CREATE_SURFACE_REPLY 0x0101u
#define LGP_MSG_COMMIT_BUFFER        0x0103u

/* Surface type constants (must match lgp-compositor/protocol/surface.h) */
#define LGP_SURFACE_CANVAS_SURFACE   5u
#define LGP_SURFACE_APPLICATION_WINDOW 4u
#define LGP_SURFACE_SHELL_SURFACE    3u

/* Layer constants */
#define LGP_LAYER_WALLPAPER    0u
#define LGP_LAYER_APPLICATION  100u
#define LGP_LAYER_SHELL        200u

/* XRGB8888 pixel format (four_cc: 'X', 'R', '2', '4') */
#define LGP_PIXEL_FORMAT_XRGB8888 0x34325258u

/* lgui_window_t struct is defined in include/window_private.h */

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

static bool lgui_read_exact(int fd, uint8_t *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, buf + got, len - got);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        got += (size_t)n;
    }
    return true;
}

/* Send a COMMIT_BUFFER message with the memfd passed as SCM_RIGHTS ancillary data. */
static bool lgui_send_commit(int lgp_fd, uint32_t surface_id,
                             uint32_t width, uint32_t height,
                             uint32_t stride, size_t byte_size, int buffer_fd) {
    /* COMMIT_BUFFER payload: surface_id, width, height, stride, pixel_fmt, byte_size */
    const uint32_t payload_len  = 6u * 4u;  /* 6 × uint32_t */
    const uint32_t total_len    = (uint32_t)LGP_HEADER_SIZE + payload_len;

    uint8_t cbuf[LGP_HEADER_SIZE + 6u * 4u];
    write_u16_le(cbuf + 0, (uint16_t)LGP_MSG_COMMIT_BUFFER);
    write_u32_le(cbuf + 2, total_len);
    write_u32_le(cbuf + 6,  surface_id);
    write_u32_le(cbuf + 10, width);
    write_u32_le(cbuf + 14, height);
    write_u32_le(cbuf + 18, stride);
    write_u32_le(cbuf + 22, LGP_PIXEL_FORMAT_XRGB8888);
    write_u32_le(cbuf + 26, (uint32_t)byte_size);

    /* Pass the buffer fd via SCM_RIGHTS */
    struct msghdr msg = {0};
    struct iovec  iov = { .iov_base = cbuf, .iov_len = sizeof(cbuf) };
    msg.msg_iov    = &iov;
    msg.msg_iovlen = 1;

    uint8_t cmsg_buf[CMSG_SPACE(sizeof(int))];
    memset(cmsg_buf, 0, sizeof(cmsg_buf));
    msg.msg_control    = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &buffer_fd, sizeof(int));

    ssize_t sent = sendmsg(lgp_fd, &msg, 0);
    return sent == (ssize_t)sizeof(cbuf);
}

/* ---------------------------------------------------------------------------
 * Rendering — paint widget tree into the memfd pixel buffer.
 * ---------------------------------------------------------------------------*/

static void lgui_window_render_internal(lgui_window_t *win) {
    if (!win || !win->canvas) return;

    /* Fill with Mahina dark background (#1E1E28) */
    lgui_canvas_fill_rect(win->canvas, 0, 0, (int)win->width, (int)win->height, 0xFF1E1E28u);

    if (win->root_widget) {
        lgui_widget_t *root = win->root_widget;
        if (root->width  == 0) root->width  = win->width;
        if (root->height == 0) root->height = win->height;
        lgui_widget_render(win->canvas, root, 0, 0);
    }

    win->dirty = false;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

lgui_window_t *lgui_window_create(lgui_application_t *app,
                                   uint32_t width, uint32_t height,
                                   uint32_t layer) {
    if (!app || width == 0 || height == 0) return NULL;

    lgui_window_t *win = calloc(1, sizeof(lgui_window_t));
    if (!win) return NULL;

    win->app         = app;
    win->width       = width;
    win->height      = height;
    win->layer       = layer;
    win->buffer_fd   = -1;
    win->dirty       = true;

    /* Allocate a memfd for the shared pixel buffer */
    char memfd_name[64];
    snprintf(memfd_name, sizeof(memfd_name), "lunagui_win_%u", layer);

    win->buffer_size = (size_t)width * height * 4u;
    win->buffer_fd   = memfd_create(memfd_name, MFD_CLOEXEC);
    if (win->buffer_fd < 0) {
        free(win);
        return NULL;
    }

    if (ftruncate(win->buffer_fd, (off_t)win->buffer_size) < 0) {
        close(win->buffer_fd);
        free(win);
        return NULL;
    }

    win->buffer_map = mmap(NULL, win->buffer_size,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           win->buffer_fd, 0);
    if (win->buffer_map == MAP_FAILED) {
        close(win->buffer_fd);
        free(win);
        return NULL;
    }

    win->canvas = lgui_canvas_create(win->buffer_map, win->width, win->height, win->width * 4u);
    if (!win->canvas) {
        munmap(win->buffer_map, win->buffer_size);
        close(win->buffer_fd);
        free(win);
        return NULL;
    }

    /* Register with the application so the event loop can repaint dirty windows */
    if (app->window_count < 16) {
        app->windows[app->window_count++] = win;
    }

    return win;
}

void lgui_window_set_root_widget(lgui_window_t *win, lgui_widget_t *widget) {
    if (!win) return;
    win->root_widget = widget;
    win->dirty = true; /* Widget tree changed — schedule repaint */
}

void lgui_window_show(lgui_window_t *win) {
    if (!win || !win->app || win->app->lgp_fd < 0) return;

    int lgp_fd = win->app->lgp_fd;

    /* --- Step 1: Send CREATE_SURFACE ------------------------------------ */
    /* Determine surface type from layer */
    uint32_t surface_type;
    if (win->layer == LGP_LAYER_WALLPAPER) {
        surface_type = LGP_SURFACE_CANVAS_SURFACE;
    } else if (win->layer >= LGP_LAYER_SHELL) {
        surface_type = LGP_SURFACE_SHELL_SURFACE;
    } else {
        surface_type = LGP_SURFACE_APPLICATION_WINDOW;
    }

    /* CREATE_SURFACE payload: surface_type, x, y, width, height, layer */
    const uint32_t cs_payload_len = 6u * 4u;
    const uint32_t cs_total_len   = (uint32_t)LGP_HEADER_SIZE + cs_payload_len;

    uint8_t csbuf[LGP_HEADER_SIZE + 6u * 4u];
    write_u16_le(csbuf + 0, (uint16_t)LGP_MSG_CREATE_SURFACE);
    write_u32_le(csbuf + 2, cs_total_len);
    write_u32_le(csbuf + 6,  surface_type);
    write_u32_le(csbuf + 10, 0u); /* x */
    write_u32_le(csbuf + 14, 0u); /* y */
    write_u32_le(csbuf + 18, win->width);
    write_u32_le(csbuf + 22, win->height);
    write_u32_le(csbuf + 26, win->layer);

    if (!lgui_write_all(lgp_fd, csbuf, sizeof(csbuf))) {
        fprintf(stderr, "lunagui: failed to send CREATE_SURFACE\n");
        return;
    }

    /* --- Step 2: Read CREATE_SURFACE_REPLY (synchronous) ---------------- */
    uint8_t reply_hdr[LGP_HEADER_SIZE];
    if (!lgui_read_exact(lgp_fd, reply_hdr, sizeof(reply_hdr))) {
        fprintf(stderr, "lunagui: failed to read CREATE_SURFACE_REPLY header\n");
        return;
    }

    uint16_t reply_type = (uint16_t)(reply_hdr[0] | (reply_hdr[1] << 8));
    uint32_t reply_len  = read_u32_le(reply_hdr + 2);

    if (reply_type != LGP_MSG_CREATE_SURFACE_REPLY) {
        fprintf(stderr, "lunagui: expected CREATE_SURFACE_REPLY, got 0x%04X\n", reply_type);
        return;
    }

    if (reply_len < LGP_HEADER_SIZE + 8u) {
        fprintf(stderr, "lunagui: CREATE_SURFACE_REPLY too short\n");
        return;
    }

    uint8_t reply_payload[8u];
    if (!lgui_read_exact(lgp_fd, reply_payload, sizeof(reply_payload))) {
        fprintf(stderr, "lunagui: failed to read CREATE_SURFACE_REPLY payload\n");
        return;
    }

    uint32_t status     = read_u32_le(reply_payload + 0);
    uint32_t surface_id = read_u32_le(reply_payload + 4);

    if (status != 0u) {
        fprintf(stderr, "lunagui: CREATE_SURFACE rejected (status=%u)\n", status);
        return;
    }

    /* Drain any extra payload bytes the reply might have */
    if (reply_len > LGP_HEADER_SIZE + 8u) {
        uint32_t extra = reply_len - (uint32_t)LGP_HEADER_SIZE - 8u;
        uint8_t  discard[64];
        while (extra > 0u) {
            uint32_t n = extra < sizeof(discard) ? extra : (uint32_t)sizeof(discard);
            if (!lgui_read_exact(lgp_fd, discard, n)) break;
            extra -= n;
        }
    }

    win->surface_id = surface_id;

    /* --- Step 3: Render widget tree into the shared buffer -------------- */
    lgui_window_render_internal(win);

    /* --- Step 4: Send COMMIT_BUFFER ------------------------------------- */
    if (!lgui_send_commit(lgp_fd, surface_id, win->width, win->height,
                          win->width * 4u, win->buffer_size, win->buffer_fd)) {
        fprintf(stderr, "lunagui: failed to send COMMIT_BUFFER\n");
    }
}

lgui_canvas_t *lgui_window_get_canvas(lgui_window_t *win) {
    if (!win) return NULL;
    return win->canvas;
}

void lgui_window_update(lgui_window_t *win) {
    if (!win || !win->app || win->app->lgp_fd < 0) return;
    if (win->surface_id == 0u) return; /* Window not yet shown */

    lgui_window_render_internal(win);

    lgui_send_commit(win->app->lgp_fd,
                     win->surface_id,
                     win->width, win->height,
                     win->width * 4u,
                     win->buffer_size,
                     win->buffer_fd);
}
