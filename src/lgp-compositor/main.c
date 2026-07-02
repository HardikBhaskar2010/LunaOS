/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "config/args.h"
#include "logging/log.h"
#include "util/signal.h"
#include "drm/device.h"
#include "drm/connector.h"
#include "kms/crtc.h"
#include "kms/page_flip.h"
#include "kms/dumb_buffer.h"
#include "ipc/socket_server.h"
#include "ipc/client.h"
#include "protocol/tlv.h"
#include "protocol/hello.h"
#include "protocol/caps.h"
#include "protocol/surface.h"
#include "protocol/wm.h"
#include "scene/surface.h"
#include "input/input.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 16
#define RUNTIME_LOG_PATH "/var/log/luna-init/runtime.log"

#include "main.h"

static bool lgp_write_all(int fd, const uint8_t *buf, size_t len);

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void lgp_send_pointer_motion_to_session(lgp_compositor_state_t *state,
                                               uint32_t session_id,
                                               int x,
                                               int y) {
    if (!state || session_id == 0u) return;

    lgp_client_t *client = state->clients;
    while (client) {
        if (client->session_id == session_id) {
            uint8_t payload[8];
            write_u32_le(payload + 0, (uint32_t)x);
            write_u32_le(payload + 4, (uint32_t)y);

            uint8_t frame[LGP_HEADER_SIZE + 8];
            lgp_tlv_encode_header(frame, sizeof(frame), LGP_MSG_POINTER_MOTION, sizeof(frame));
            memcpy(frame + LGP_HEADER_SIZE, payload, 8);
            lgp_write_all(client->fd, frame, sizeof(frame));
            return;
        }
        client = client->next;
    }
}

static void lgp_send_pointer_button_to_session(lgp_compositor_state_t *state,
                                               uint32_t session_id,
                                               uint8_t button,
                                               bool pressed) {
    if (!state || session_id == 0u) return;

    lgp_client_t *client = state->clients;
    while (client) {
        if (client->session_id == session_id) {
            uint8_t payload[2];
            payload[0] = button;
            payload[1] = pressed ? 1u : 0u;

            uint8_t frame[LGP_HEADER_SIZE + 2];
            lgp_tlv_encode_header(frame, sizeof(frame), LGP_MSG_POINTER_BUTTON, sizeof(frame));
            memcpy(frame + LGP_HEADER_SIZE, payload, 2);
            lgp_write_all(client->fd, frame, sizeof(frame));
            return;
        }
        client = client->next;
    }
}

static const lgp_surface_t *lgp_find_pointer_target(lgp_compositor_state_t *state,
                                                    int x,
                                                    int y) {
    if (!state) return NULL;

    int top_layer = -1;
    const lgp_surface_t *target = NULL;

    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        lgp_surface_t *s = &state->surface_manager.surfaces[i];
        if (!s->in_use) continue;

        /* The WM shell receives global input separately. Transparent shell
         * overlays should not prevent applications below them from receiving
         * surface-local events. */
        if (state->wm_client && s->owner_session_id == state->wm_client->session_id) {
            continue;
        }
        if (s->layer < LGP_LAYER_APPLICATION || s->layer >= LGP_LAYER_SHELL) {
            continue;
        }
        if (s->wm_state == LGP_WM_STATE_HIDDEN || s->wm_state == LGP_WM_STATE_MINIMIZED) {
            continue;
        }

        if (x >= s->x && x < s->x + (int32_t)s->width &&
            y >= s->y && y < s->y + (int32_t)s->height) {
            if ((int)s->layer > top_layer) {
                top_layer = (int)s->layer;
                target = s;
            }
        }
    }

    return target;
}

void lgp_dispatch_pointer_motion(lgp_compositor_state_t *state, int x, int y) {
    if (!state) return;

    if (state->wm_client) {
        lgp_send_pointer_motion_to_session(state, state->wm_client->session_id, x, y);
    }

    const lgp_surface_t *target = lgp_find_pointer_target(state, x, y);
    if (target) {
        lgp_send_pointer_motion_to_session(state,
                                           target->owner_session_id,
                                           x - target->x,
                                           y - target->y);
    }
}

void lgp_dispatch_pointer_button(lgp_compositor_state_t *state, int x, int y, uint8_t button, bool pressed) {
    if (!state) return;

    if (state->wm_client) {
        lgp_send_pointer_button_to_session(state, state->wm_client->session_id, button, pressed);
    }

    const lgp_surface_t *target = lgp_find_pointer_target(state, x, y);
    if (target) {
        if (pressed) {
            state->keyboard_focus_session_id = target->owner_session_id;
            state->keyboard_focus_surface_id = target->id;
        }

        lgp_send_pointer_button_to_session(state, target->owner_session_id, button, pressed);
    }
}

void lgp_dispatch_keyboard_key(lgp_compositor_state_t *state, uint32_t key, uint32_t key_state, uint32_t modifiers) {
    if (!state) return;

    uint32_t target_session_id = state->keyboard_focus_session_id;

    /* Check if WM grabbed this key */
    if (state->wm_client) {
        for (size_t i = 0; i < state->grabbed_keys_count; i++) {
            if (state->grabbed_keys[i].key == key && state->grabbed_keys[i].modifiers == modifiers) {
                target_session_id = state->wm_client->session_id;
                break;
            }
        }
    }

    if (target_session_id == 0) return;

    lgp_client_t *client = state->clients;
    while (client) {
        if (client->session_id == target_session_id) {
            uint8_t payload[12];
            payload[0] = key & 0xFF;
            payload[1] = (key >> 8) & 0xFF;
            payload[2] = (key >> 16) & 0xFF;
            payload[3] = (key >> 24) & 0xFF;

            payload[4] = key_state & 0xFF;
            payload[5] = (key_state >> 8) & 0xFF;
            payload[6] = (key_state >> 16) & 0xFF;
            payload[7] = (key_state >> 24) & 0xFF;

            payload[8] = modifiers & 0xFF;
            payload[9] = (modifiers >> 8) & 0xFF;
            payload[10] = (modifiers >> 16) & 0xFF;
            payload[11] = (modifiers >> 24) & 0xFF;

            uint8_t frame[LGP_HEADER_SIZE + 12];
            lgp_tlv_encode_header(frame, sizeof(frame), LGP_MSG_KEYBOARD_KEY, sizeof(frame));
            memcpy(frame + LGP_HEADER_SIZE, payload, 12);
            lgp_write_all(client->fd, frame, sizeof(frame));
            break;
        }
        client = client->next;
    }
}

static bool lgp_write_all(int fd, const uint8_t *buf, size_t len) {
    size_t written = 0;

    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            LGP_WARN("protocol", "write() failed: %s", strerror(errno));
            return false;
        }
        if (n == 0) {
            LGP_WARN("protocol", "short write while sending reply");
            return false;
        }
        written += (size_t)n;
    }

    return true;
}

static bool lgp_client_queue_fd(lgp_client_t *client, int fd) {
    if (client->pending_fd_count >= LGP_CLIENT_MAX_PENDING_FDS) {
        close(fd);
        LGP_WARN("protocol", "Client session=%u sent too many pending file descriptors",
                 client->session_id);
        return false;
    }

    client->pending_fds[client->pending_fd_count++] = fd;
    return true;
}

static int lgp_client_take_fd(lgp_client_t *client) {
    if (!client || client->pending_fd_count == 0) {
        return -1;
    }

    int fd = client->pending_fds[0];
    for (size_t i = 1; i < client->pending_fd_count; i++) {
        client->pending_fds[i - 1] = client->pending_fds[i];
    }
    client->pending_fd_count--;
    client->pending_fds[client->pending_fd_count] = -1;
    return fd;
}

static lgp_client_t *lgp_state_find_client(lgp_compositor_state_t *state, int fd) {
    for (lgp_client_t *client = state->clients; client; client = client->next) {
        if (client->fd == fd) return client;
    }
    return NULL;
}

static void lgp_state_add_client(lgp_compositor_state_t *state, lgp_client_t *client) {
    client->next = state->clients;
    state->clients = client;
}

static void lgp_state_remove_client(lgp_compositor_state_t *state, lgp_client_t *client) {
    lgp_client_t **cursor = &state->clients;

    while (*cursor) {
        if (*cursor == client) {
            *cursor = client->next;
            client->next = NULL;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static void lgp_state_close_client(lgp_compositor_state_t *state, lgp_client_t *client) {
    if (!client) return;

    LGP_DEBUG("ipc", "Disconnecting LGP client session=%u fd=%d",
              client->session_id, client->fd);
    if (state->wm_client == client) {
        state->wm_client = NULL;
    }
    epoll_ctl(state->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    lgp_surface_manager_destroy_for_client(&state->surface_manager, client);
    lgp_state_remove_client(state, client);
    lgp_client_destroy(client);
}

static void lgp_state_close_all_clients(lgp_compositor_state_t *state) {
    while (state->clients) {
        lgp_state_close_client(state, state->clients);
    }
}

static int lgp_client_read_available(lgp_client_t *client) {
    size_t available = sizeof(client->rx_buf) - client->rx_len;
    if (available == 0) {
        LGP_WARN("protocol", "Client session=%u receive buffer full", client->session_id);
        return -1;
    }

    uint8_t control[CMSG_SPACE(sizeof(int) * LGP_CLIENT_MAX_PENDING_FDS)] = {0};
    struct iovec iov = {
        .iov_base = client->rx_buf + client->rx_len,
        .iov_len = available
    };
    struct msghdr msgh = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control,
        .msg_controllen = sizeof(control)
    };

    ssize_t n = recvmsg(client->fd, &msgh, MSG_CMSG_CLOEXEC);
    if (n > 0) {
        client->rx_len += (size_t)n;
        if ((msgh.msg_flags & MSG_CTRUNC) != 0) {
            LGP_WARN("protocol", "Client session=%u sent truncated control data",
                     client->session_id);
            return -1;
        }

        for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
             cmsg;
             cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
                continue;
            }

            size_t fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            const uint8_t *fd_bytes = (const uint8_t *)CMSG_DATA(cmsg);
            for (size_t i = 0; i < fd_count; i++) {
                int received_fd = -1;
                memcpy(&received_fd, fd_bytes + (i * sizeof(received_fd)), sizeof(received_fd));
                if (!lgp_client_queue_fd(client, received_fd)) {
                    return -1;
                }
            }
        }
        return 1;
    }
    if (n == 0) {
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return 1;
    }

    LGP_WARN("protocol", "Read failed for client session=%u: %s",
             client->session_id, strerror(errno));
    return -1;
}

static bool lgp_handle_fill_rect(lgp_client_t *client, const lgp_msg_t *msg, drm_device_t *drm_dev) {
    /* Security: FILL_RECT requires LGP_CAP_DIRECT_LGP — unprivileged apps must
     * use surfaces via CREATE_SURFACE/COMMIT_BUFFER instead. */
    if (!(client->caps_granted & LGP_CAP_DIRECT_LGP)) {
        LGP_WARN("protocol", "Client session=%u attempted FILL_RECT without LGP_CAP_DIRECT_LGP",
                 client->session_id);
        return false;
    }

    size_t payload_len = msg->length - LGP_HEADER_SIZE;
    if (payload_len < 4) {
        LGP_WARN("protocol", "Client session=%u sent short FILL_RECT payload", client->session_id);
        return false;
    }

    uint8_t r = msg->payload[0];
    uint8_t g = msg->payload[1];
    uint8_t b = msg->payload[2];

    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    LGP_INFO("main", "Client session=%u requested FILL_RECT with color #%06X",
             client->session_id, color);

    uint32_t *pixels = (uint32_t *)drm_dev->front_buffer->map;
    size_t pixel_count = drm_dev->front_buffer->size / 4;
    for (size_t p = 0; p < pixel_count; p++) {
        pixels[p] = color;
    }

    if (kms_page_flip(drm_dev, drm_dev->front_buffer, NULL) != 0) {
        LGP_WARN("main", "FILL_RECT page flip failed for client session=%u", client->session_id);
        return false;
    }

    return true;
}

static bool lgp_send_create_surface_reply(lgp_client_t *client, uint32_t status, uint32_t surface_id) {
    uint8_t reply[LGP_HEADER_SIZE + 8];
    if (!lgp_surface_encode_create_reply(reply, sizeof(reply), status, surface_id)) {
        return false;
    }

    return lgp_write_all(client->fd, reply, sizeof(reply));
}

static bool lgp_handle_create_surface(lgp_compositor_state_t *state,
                                      lgp_client_t *client,
                                      const lgp_msg_t *msg,
                                      const drm_device_t *drm_dev) {
    lgp_surface_create_payload_t payload;
    if (!lgp_surface_decode_create(msg, &payload)) {
        LGP_WARN("protocol", "Client session=%u sent malformed CREATE_SURFACE", client->session_id);
        return false;
    }

    uint32_t surface_id = 0;
    uint32_t status = lgp_surface_manager_create(&state->surface_manager,
                                                 client,
                                                 &payload,
                                                 drm_dev->mode.hdisplay,
                                                 drm_dev->mode.vdisplay,
                                                 &surface_id);
    if (!lgp_send_create_surface_reply(client, status, surface_id)) {
        return false;
    }

    if (status != LGP_SURFACE_STATUS_OK) {
        LGP_WARN("surface", "Rejected CREATE_SURFACE session=%u status=%u type=%u layer=%u",
                 client->session_id, status, payload.surface_type, payload.layer);
        return true;
    }

    LGP_INFO("surface", "Client session=%u created surface id=%u", client->session_id, surface_id);

    /* Auto-focus newly created application window to route keyboard inputs */
    if (payload.layer == LGP_LAYER_APPLICATION) {
        state->keyboard_focus_session_id = client->session_id;
        state->keyboard_focus_surface_id = surface_id;
        LGP_INFO("surface", "Auto-focused client session=%u surface id=%u", client->session_id, surface_id);
    }
    
    if (state->wm_client && state->wm_client != client) {
        uint8_t buf[64];
        if (lgp_wm_encode_surface_created(buf, sizeof(buf), surface_id, payload.surface_type, payload.width, payload.height)) {
            lgp_write_all(state->wm_client->fd, buf, LGP_HEADER_SIZE + 16);
        }
    }
    
    return true;
}

static bool lgp_handle_destroy_surface(lgp_compositor_state_t *state,
                                       lgp_client_t *client,
                                       const lgp_msg_t *msg) {
    lgp_surface_destroy_payload_t payload;
    if (!lgp_surface_decode_destroy(msg, &payload)) {
        LGP_WARN("protocol", "Client session=%u sent malformed DESTROY_SURFACE", client->session_id);
        return false;
    }

    uint32_t status = lgp_surface_manager_destroy(&state->surface_manager, client, payload.surface_id);
    if (status != LGP_SURFACE_STATUS_OK) {
        LGP_WARN("surface", "Rejected DESTROY_SURFACE session=%u surface=%u status=%u",
                 client->session_id, payload.surface_id, status);
        return false;
    }

    if (state->wm_client && state->wm_client != client) {
        uint8_t buf[32];
        if (lgp_wm_encode_surface_destroyed(buf, sizeof(buf), payload.surface_id)) {
            lgp_write_all(state->wm_client->fd, buf, LGP_HEADER_SIZE + 4);
        }
    }

    return true;
}

static bool lgp_repaint_surfaces(lgp_compositor_state_t *state, drm_device_t *drm_dev) {
    if (lgp_surface_manager_composite(&state->surface_manager,
                                      drm_dev->front_buffer->map,
                                      drm_dev->front_buffer->width,
                                      drm_dev->front_buffer->height,
                                      drm_dev->front_buffer->pitch) != 0) {
        LGP_WARN("surface", "Failed to composite surface scene");
        return false;
    }

    int flip_res = kms_page_flip(drm_dev, drm_dev->front_buffer, NULL);
    if (flip_res != 0 && flip_res != -EBUSY) {
        LGP_WARN("surface", "Surface scene page flip failed");
        return false;
    }

    return true;
}

static bool lgp_handle_commit_buffer(lgp_compositor_state_t *state,
                                     lgp_client_t *client,
                                     const lgp_msg_t *msg,
                                     drm_device_t *drm_dev) {
    lgp_surface_commit_payload_t payload;
    if (!lgp_surface_decode_commit(msg, &payload)) {
        LGP_WARN("protocol", "Client session=%u sent malformed COMMIT_BUFFER", client->session_id);
        return false;
    }

    int buffer_fd = lgp_client_take_fd(client);
    uint32_t status = lgp_surface_manager_commit(&state->surface_manager, client, &payload, buffer_fd);
    if (status != LGP_SURFACE_STATUS_OK) {
        LGP_WARN("surface", "Rejected COMMIT_BUFFER session=%u surface=%u status=%u",
                 client->session_id, payload.surface_id, status);
        return false;
    }

    LGP_INFO("surface", "Client session=%u committed surface id=%u",
             client->session_id, payload.surface_id);
    return lgp_repaint_surfaces(state, drm_dev);
}

static bool lgp_handle_wm_set_position(lgp_compositor_state_t *state, lgp_client_t *client, const lgp_msg_t *msg, drm_device_t *drm_dev) {
    if (!(client->caps_granted & LGP_CAP_WINDOW_MANAGER)) return false; /* Must be WM */
    lgp_wm_set_position_payload_t payload;
    if (!lgp_wm_decode_set_position(msg, &payload)) return false;

    // Find the surface
    lgp_surface_t *surf = NULL;
    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        if (state->surface_manager.surfaces[i].in_use && state->surface_manager.surfaces[i].id == payload.surface_id) {
            surf = &state->surface_manager.surfaces[i];
            break;
        }
    }
    if (!surf) return true;

    /* Validate bounds */
    int max_w = drm_dev ? (int)drm_dev->mode.hdisplay : 4096;
    int max_h = drm_dev ? (int)drm_dev->mode.vdisplay : 4096;
    if (payload.x < -(int)surf->width || payload.x + (int)surf->width > max_w ||
        payload.y < -(int)surf->height || payload.y + (int)surf->height > max_h) {
        LGP_WARN("wm", "WM rejected surface position (%d, %d) out of bounds", payload.x, payload.y);
        return true; /* Keep the client connected, just drop the out of bounds move */
    }

    surf->x = payload.x;
    surf->y = payload.y;
    return lgp_repaint_surfaces(state, drm_dev);
}

static bool lgp_handle_wm_set_focus(lgp_compositor_state_t *state, lgp_client_t *client, const lgp_msg_t *msg) {
    if (!(client->caps_granted & LGP_CAP_WINDOW_MANAGER)) return false;
    lgp_wm_set_focus_payload_t payload;
    if (!lgp_wm_decode_set_focus(msg, &payload)) return false;
    
    if (payload.surface_id == 0) {
        state->keyboard_focus_session_id = 0;
        state->keyboard_focus_surface_id = 0;
    } else {
        const lgp_surface_t *surf = lgp_surface_manager_find(&state->surface_manager, payload.surface_id);
        if (surf) {
            state->keyboard_focus_session_id = surf->owner_session_id;
            state->keyboard_focus_surface_id = surf->id;
        } else {
            LGP_WARN("wm", "WM set focus to unknown surface_id %u", payload.surface_id);
            return false;
        }
    }
    return true;
}

static bool lgp_handle_wm_set_state(lgp_compositor_state_t *state, lgp_client_t *client, const lgp_msg_t *msg, drm_device_t *drm_dev) {
    if (!(client->caps_granted & LGP_CAP_WINDOW_MANAGER)) return false;
    lgp_wm_set_state_payload_t payload;
    if (!lgp_wm_decode_set_state(msg, &payload)) return false;
    
    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        if (state->surface_manager.surfaces[i].in_use && state->surface_manager.surfaces[i].id == payload.surface_id) {
            state->surface_manager.surfaces[i].wm_state = payload.state;
            break;
        }
    }
    return lgp_repaint_surfaces(state, drm_dev);
}

static bool lgp_handle_wm_grab_key(lgp_compositor_state_t *state, lgp_client_t *client, const lgp_msg_t *msg) {
    if (!(client->caps_granted & LGP_CAP_WINDOW_MANAGER)) return false;
    lgp_wm_grab_key_payload_t payload;
    if (!lgp_wm_decode_grab_key(msg, &payload)) return false;
    
    if (state->grabbed_keys_count < 128) {
        state->grabbed_keys[state->grabbed_keys_count].key = payload.key;
        state->grabbed_keys[state->grabbed_keys_count].modifiers = payload.modifiers;
        state->grabbed_keys_count++;
        LGP_INFO("ipc", "WM registered grab for key=%u mods=%u", payload.key, payload.modifiers);
    } else {
        LGP_WARN("ipc", "WM grab key limit reached");
    }
    return true;
}

static bool lgp_handle_client_message(lgp_compositor_state_t *state,
                                      lgp_client_t *client,
                                      const lgp_msg_t *msg,
                                      drm_device_t *drm_dev) {
    if (!client->hello_done && msg->type != LGP_MSG_HELLO) {
        LGP_WARN("protocol", "Client session=%u sent message 0x%04X before LGP_HELLO",
                 client->session_id, msg->type);
        return false;
    }

    if (client->hello_done && msg->type == LGP_MSG_HELLO) {
        LGP_WARN("protocol", "Client session=%u repeated LGP_HELLO", client->session_id);
        return false;
    }

    if (msg->type == LGP_MSG_HELLO) {
        bool ok = lgp_hello_handle(client, msg);
        if (ok) {
            if (client->caps_granted & LGP_CAP_WINDOW_MANAGER) {
                state->wm_client = client;
                LGP_INFO("ipc", "Client session=%u registered as Window Manager", client->session_id);
            }
            /* Send output geometry */
            uint8_t geom[LGP_HEADER_SIZE + 8];
            lgp_tlv_encode_header(geom, sizeof(geom), LGP_MSG_OUTPUT_GEOMETRY, sizeof(geom));
            uint32_t w = drm_dev ? drm_dev->mode.hdisplay : 1024;
            uint32_t h = drm_dev ? drm_dev->mode.vdisplay : 768;
            geom[LGP_HEADER_SIZE + 0] = (uint8_t)(w & 0xFF);
            geom[LGP_HEADER_SIZE + 1] = (uint8_t)((w >> 8) & 0xFF);
            geom[LGP_HEADER_SIZE + 2] = (uint8_t)((w >> 16) & 0xFF);
            geom[LGP_HEADER_SIZE + 3] = (uint8_t)((w >> 24) & 0xFF);
            geom[LGP_HEADER_SIZE + 4] = (uint8_t)(h & 0xFF);
            geom[LGP_HEADER_SIZE + 5] = (uint8_t)((h >> 8) & 0xFF);
            geom[LGP_HEADER_SIZE + 6] = (uint8_t)((h >> 16) & 0xFF);
            geom[LGP_HEADER_SIZE + 7] = (uint8_t)((h >> 24) & 0xFF);
            lgp_write_all(client->fd, geom, sizeof(geom));
        }
        return ok;
    }

    if (msg->type == LGP_MSG_FILL_RECT) {
        return lgp_handle_fill_rect(client, msg, drm_dev);
    }

    if (msg->type == LGP_MSG_CREATE_SURFACE) {
        return lgp_handle_create_surface(state, client, msg, drm_dev);
    }

    if (msg->type == LGP_MSG_DESTROY_SURFACE) {
        return lgp_handle_destroy_surface(state, client, msg);
    }

    if (msg->type == LGP_MSG_COMMIT_BUFFER) {
        return lgp_handle_commit_buffer(state, client, msg, drm_dev);
    }

    if (msg->type == LGP_MSG_CLIPBOARD_SET) {
        if (!(client->caps_granted & LGP_CAP_CLIPBOARD)) {
            LGP_WARN("protocol", "Client session=%u attempted CLIPBOARD_SET without capability", client->session_id);
            return false;
        }
        size_t payload_len = msg->length - LGP_HEADER_SIZE;
        if (payload_len >= sizeof(state->global_clipboard)) {
            payload_len = sizeof(state->global_clipboard) - 1;
        }
        memcpy(state->global_clipboard, msg->payload, payload_len);
        state->global_clipboard[payload_len] = '\0';
        LGP_INFO("ipc", "Client session=%u set clipboard (len=%zu)", client->session_id, payload_len);
        return true;
    }

    if (msg->type == LGP_MSG_CLIPBOARD_GET) {
        if (!(client->caps_granted & LGP_CAP_CLIPBOARD)) {
            LGP_WARN("protocol", "Client session=%u attempted CLIPBOARD_GET without capability", client->session_id);
            return false;
        }
        size_t cb_len = strlen(state->global_clipboard);
        uint8_t *frame = malloc(LGP_HEADER_SIZE + cb_len + 1);
        if (frame) {
            lgp_tlv_encode_header(frame, LGP_HEADER_SIZE + cb_len + 1, LGP_MSG_CLIPBOARD_DATA, LGP_HEADER_SIZE + cb_len + 1);
            memcpy(frame + LGP_HEADER_SIZE, state->global_clipboard, cb_len + 1);
            lgp_write_all(client->fd, frame, LGP_HEADER_SIZE + cb_len + 1);
            free(frame);
            LGP_INFO("ipc", "Client session=%u got clipboard (len=%zu)", client->session_id, cb_len);
        }
        return true;
    }

    if (msg->type == LGP_MSG_WM_SET_SURFACE_POSITION) {
        return lgp_handle_wm_set_position(state, client, msg, drm_dev);
    }
    if (msg->type == LGP_MSG_WM_SET_FOCUS) {
        return lgp_handle_wm_set_focus(state, client, msg);
    }
    if (msg->type == LGP_MSG_WM_SET_STATE) {
        return lgp_handle_wm_set_state(state, client, msg, drm_dev);
    }
    if (msg->type == LGP_MSG_WM_GRAB_KEY) {
        return lgp_handle_wm_grab_key(state, client, msg);
    }

    LGP_WARN("protocol", "Client session=%u sent unknown message 0x%04X",
             client->session_id, msg->type);
    return false;
}

static bool lgp_client_process_messages(lgp_compositor_state_t *state,
                                        lgp_client_t *client,
                                        drm_device_t *drm_dev) {
    while (client->rx_len >= LGP_HEADER_SIZE) {
        uint16_t type = 0;
        uint32_t length = 0;

        if (!lgp_tlv_peek_header(client->rx_buf, client->rx_len, &type, &length)) {
            return true;
        }

        if (length < LGP_HEADER_SIZE || length > sizeof(client->rx_buf)) {
            LGP_WARN("protocol", "Client session=%u sent invalid TLV length %u for type 0x%04X",
                     client->session_id, length, type);
            return false;
        }

        if (client->rx_len < length) {
            return true;
        }

        lgp_msg_t msg;
        if (!lgp_tlv_decode(client->rx_buf, length, &msg)) {
            LGP_WARN("protocol", "Client session=%u sent malformed TLV frame",
                     client->session_id);
            return false;
        }

        if (!lgp_handle_client_message(state, client, &msg, drm_dev)) {
            return false;
        }

        size_t remaining = client->rx_len - length;
        if (remaining > 0) {
            memmove(client->rx_buf, client->rx_buf + length, remaining);
        }
        client->rx_len = remaining;
    }

    return true;
}

int main(int argc, char **argv) {
    lgp_args_t args;
    if (lgp_args_parse(argc, argv, &args) != 0) {
        return 1;
    }

    if (args.help) {
        printf("Usage: lgp-compositor [OPTIONS]\n");
        printf("Options:\n");
        printf("  -h, --help     Show this help message\n");
        printf("  -v, --version  Show version information\n");
        return 0;
    }

    if (args.version) {
        printf("lgp-compositor v0.1\n");
        return 0;
    }

    /* Initialize logging */
    if (lgp_log_init(RUNTIME_LOG_PATH) != 0) {
        fprintf(stderr, "lgp-compositor: failed to initialize log\n");
        return 1;
    }

    LGP_INFO("main", "Starting lgp-compositor (Mahina OS)");

    lgp_compositor_state_t state = {
        .epoll_fd = epoll_create1(EPOLL_CLOEXEC),
        .signal_fd = lgp_signal_init(),
        .running = true,
        .next_session_id = 1,
        .clients = NULL,
        .surface_manager = {0}
    };
    lgp_surface_manager_init(&state.surface_manager);

    if (state.epoll_fd < 0 || state.signal_fd < 0) {
        LGP_FATAL("main", "Failed to initialize epoll/signalfd");
        lgp_log_close();
        return 1;
    }
    /* Register signalfd with epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = state.signal_fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, state.signal_fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add signalfd to epoll");
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* M1: Raw DRM/KMS Modesetting */
    drm_device_t drm_dev;
    if (drm_device_open(&drm_dev, "/dev/dri/card0") != 0) {
        LGP_FATAL("main", "DRM device lost (could not open)");
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    if (drm_connector_setup(&drm_dev) != 0) {
        LGP_FATAL("main", "Failed to find suitable DRM connector/mode");
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* Input backend: libinput when available, raw evdev fallback otherwise. */
    lgp_input_init((uint32_t)drm_dev.mode.hdisplay, (uint32_t)drm_dev.mode.vdisplay);
    int input_fd = lgp_input_get_fd();
    if (input_fd >= 0) {
        struct epoll_event ev_input = {0};
        ev_input.events = EPOLLIN;
        ev_input.data.fd = input_fd;
        epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, input_fd, &ev_input);
    }

    int input_aux_fd = lgp_input_get_aux_fd();
    if (input_aux_fd >= 0) {
        struct epoll_event ev_input_aux = {0};
        ev_input_aux.events = EPOLLIN;
        ev_input_aux.data.fd = input_aux_fd;
        epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, input_aux_fd, &ev_input_aux);
    }

    drm_dev.front_buffer = kms_dumb_buffer_create(&drm_dev, drm_dev.mode.hdisplay, drm_dev.mode.vdisplay);
    if (!drm_dev.front_buffer) {
        LGP_FATAL("main", "Failed to create front buffer");
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* Paint the first frame solid black (LUNA Void) per DL-043 */
    memset(drm_dev.front_buffer->map, 0, drm_dev.front_buffer->size);

    if (kms_crtc_commit(&drm_dev, drm_dev.front_buffer) != 0) {
        LGP_FATAL("main", "Failed to commit CRTC mode");
        kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* Add DRM FD to epoll to handle page flip events */
    ev.events = EPOLLIN;
    ev.data.fd = drm_dev.fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, drm_dev.fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add DRM fd to epoll");
    }

    /* M2: Socket server */
    int listen_fd = lgp_socket_server_init();
    if (listen_fd < 0) {
        LGP_FATAL("main", "Failed to initialize socket server");
        kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add listen fd to epoll");
    }

    /* Main Event Loop */
    struct epoll_event events[MAX_EVENTS];
    while (state.running) {
        int n = epoll_wait(state.epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            continue; /* EINTR handled implicitly */
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == state.signal_fd) {
                lgp_signal_action_t action = lgp_signal_read(state.signal_fd);
                if (action == LGP_SIGNAL_SHUTDOWN) {
                    LGP_INFO("main", "Initiating shutdown");
                    state.running = false;
                }
            } else if (fd == drm_dev.fd) {
                kms_handle_events(&drm_dev);
            } else if ((input_fd >= 0 && fd == input_fd) ||
                       (input_aux_fd >= 0 && fd == input_aux_fd)) {
                lgp_input_pump(&state);
            } else if (fd == listen_fd) {
                int client_fd = lgp_socket_server_accept(listen_fd);
                if (client_fd >= 0) {
                    lgp_client_t *client = lgp_client_create(client_fd);
                    if (!client) {
                        close(client_fd);
                        continue;
                    }

                    client->session_id = state.next_session_id++;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        lgp_client_destroy(client);
                        continue;
                    }

                    lgp_state_add_client(&state, client);
                    LGP_DEBUG("ipc", "Client session=%u registered", client->session_id);
                }
            } else {
                lgp_client_t *client = lgp_state_find_client(&state, fd);
                if (!client) {
                    LGP_WARN("ipc", "Received event for unknown client fd %d", fd);
                    epoll_ctl(state.epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                int read_status = lgp_client_read_available(client);
                if (read_status <= 0) {
                    lgp_state_close_client(&state, client);
                    continue;
                }

                if (!lgp_client_process_messages(&state, client, &drm_dev)) {
                    lgp_state_close_client(&state, client);
                }
            }
        }
    }

    /* Clean up */
    LGP_INFO("main", "lgp-compositor shutting down cleanly");

    lgp_state_close_all_clients(&state);
    lgp_input_cleanup();
    close(listen_fd);
    unlink(LGP_SOCKET_PATH);
    
    kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
    drm_device_close(&drm_dev);
    close(state.signal_fd);
    close(state.epoll_fd);
    lgp_log_close();

    return 0;
}
