/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "surface.h"
#include "../logging/log.h"
#include "../protocol/caps.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define LGP_BYTES_PER_PIXEL_XRGB8888 4u
#define LGP_LUNA_VOID_XRGB8888 0x000A0A0Fu

static void lgp_surface_release(lgp_surface_t *surface) {
    if (!surface) return;

    if (surface->buffer_map) {
        munmap(surface->buffer_map, surface->buffer_size);
    }
    memset(surface, 0, sizeof(*surface));
}

static bool lgp_surface_type_known(uint32_t surface_type) {
    return surface_type >= LGP_SURFACE_SYSTEM_CHROME &&
           surface_type <= LGP_SURFACE_OVERLAY_SURFACE;
}

static bool lgp_surface_is_in_display(const lgp_surface_create_payload_t *payload,
                                      uint32_t display_width,
                                      uint32_t display_height) {
    if (payload->width == 0 || payload->height == 0) {
        return false;
    }
    if (payload->x < 0 || payload->y < 0) {
        return false;
    }
    if (payload->width > display_width || payload->height > display_height) {
        return false;
    }

    uint32_t x = (uint32_t)payload->x;
    uint32_t y = (uint32_t)payload->y;
    return x <= display_width - payload->width &&
           y <= display_height - payload->height;
}

static bool lgp_surface_client_has_cap(const lgp_client_t *client, uint32_t cap) {
    return client && ((client->caps_granted & cap) == cap);
}

static uint32_t lgp_surface_validate_request(const lgp_client_t *client,
                                             const lgp_surface_create_payload_t *payload,
                                             uint32_t display_width,
                                             uint32_t display_height) {
    if (!client || !payload || !lgp_surface_type_known(payload->surface_type)) {
        return LGP_SURFACE_STATUS_INVALID;
    }
    if (!lgp_surface_is_in_display(payload, display_width, display_height)) {
        return LGP_SURFACE_STATUS_INVALID;
    }

    switch (payload->surface_type) {
        case LGP_SURFACE_APPLICATION_WINDOW:
            return payload->layer == LGP_LAYER_APPLICATION ?
                   LGP_SURFACE_STATUS_OK : LGP_SURFACE_STATUS_INVALID;
        case LGP_SURFACE_CANVAS_SURFACE:
            if (!lgp_surface_client_has_cap(client, LGP_CAP_CANVAS_SURFACE)) {
                return LGP_SURFACE_STATUS_DENIED;
            }
            return (payload->layer == LGP_LAYER_APPLICATION ||
                    payload->layer == LGP_LAYER_WALLPAPER) ?
                   LGP_SURFACE_STATUS_OK : LGP_SURFACE_STATUS_INVALID;
        case LGP_SURFACE_LUNA_ISLAND:
            if (!lgp_surface_client_has_cap(client, LGP_CAP_LUNA_ISLAND)) {
                return LGP_SURFACE_STATUS_DENIED;
            }
            return payload->layer == LGP_LAYER_LUNA_ISLAND ?
                   LGP_SURFACE_STATUS_OK : LGP_SURFACE_STATUS_INVALID;
        case LGP_SURFACE_SYSTEM_CHROME:
        case LGP_SURFACE_SHELL_SURFACE:
        case LGP_SURFACE_OVERLAY_SURFACE:
            if (!lgp_surface_client_has_cap(client, LGP_CAP_LAYER_SHELL)) {
                return LGP_SURFACE_STATUS_DENIED;
            }
            return payload->layer >= LGP_LAYER_SHELL &&
                   payload->layer <= LGP_LAYER_SYSTEM_MODAL ?
                   LGP_SURFACE_STATUS_OK : LGP_SURFACE_STATUS_INVALID;
        default:
            return LGP_SURFACE_STATUS_INVALID;
    }
}

static lgp_surface_t *lgp_surface_manager_find_mut(lgp_surface_manager_t *manager,
                                                   uint32_t surface_id) {
    if (!manager) return NULL;

    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        if (manager->surfaces[i].in_use && manager->surfaces[i].id == surface_id) {
            return &manager->surfaces[i];
        }
    }
    return NULL;
}

void lgp_surface_manager_init(lgp_surface_manager_t *manager) {
    if (!manager) return;

    memset(manager, 0, sizeof(*manager));
    manager->next_surface_id = 1;
}

uint32_t lgp_surface_manager_create(lgp_surface_manager_t *manager,
                                    const lgp_client_t *client,
                                    const lgp_surface_create_payload_t *payload,
                                    uint32_t display_width,
                                    uint32_t display_height,
                                    uint32_t *out_surface_id) {
    if (out_surface_id) {
        *out_surface_id = 0;
    }
    if (!manager) {
        return LGP_SURFACE_STATUS_INVALID;
    }

    uint32_t status = lgp_surface_validate_request(client, payload, display_width, display_height);
    if (status != LGP_SURFACE_STATUS_OK) {
        return status;
    }

    lgp_surface_t *slot = NULL;
    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        if (!manager->surfaces[i].in_use) {
            slot = &manager->surfaces[i];
            break;
        }
    }
    if (!slot) {
        return LGP_SURFACE_STATUS_NO_MEMORY;
    }

    slot->in_use = true;
    slot->id = manager->next_surface_id++;
    if (manager->next_surface_id == 0) {
        manager->next_surface_id = 1;
    }
    slot->owner_session_id = client->session_id;
    slot->surface_type = payload->surface_type;
    slot->x = payload->x;
    slot->y = payload->y;
    slot->width = payload->width;
    slot->height = payload->height;
    slot->layer = payload->layer;

    if (out_surface_id) {
        *out_surface_id = slot->id;
    }

    LGP_DEBUG("surface", "Created surface id=%u session=%u type=%u layer=%u %ux%u at %d,%d",
              slot->id, slot->owner_session_id, slot->surface_type, slot->layer,
              slot->width, slot->height, slot->x, slot->y);
    return LGP_SURFACE_STATUS_OK;
}

uint32_t lgp_surface_manager_destroy(lgp_surface_manager_t *manager,
                                     const lgp_client_t *client,
                                     uint32_t surface_id) {
    lgp_surface_t *surface = lgp_surface_manager_find_mut(manager, surface_id);
    if (!surface) {
        return LGP_SURFACE_STATUS_NOT_FOUND;
    }
    if (!client || surface->owner_session_id != client->session_id) {
        return LGP_SURFACE_STATUS_DENIED;
    }

    LGP_DEBUG("surface", "Destroyed surface id=%u session=%u", surface->id, surface->owner_session_id);
    lgp_surface_release(surface);
    return LGP_SURFACE_STATUS_OK;
}

uint32_t lgp_surface_manager_commit(lgp_surface_manager_t *manager,
                                    const lgp_client_t *client,
                                    const lgp_surface_commit_payload_t *payload,
                                    int buffer_fd) {
    if (buffer_fd < 0) {
        return LGP_SURFACE_STATUS_BAD_BUFFER;
    }

    lgp_surface_t *surface = lgp_surface_manager_find_mut(manager, payload ? payload->surface_id : 0);
    if (!surface) {
        close(buffer_fd);
        return LGP_SURFACE_STATUS_NOT_FOUND;
    }
    if (!client || surface->owner_session_id != client->session_id) {
        close(buffer_fd);
        return LGP_SURFACE_STATUS_DENIED;
    }
    if (!payload ||
        payload->width != surface->width ||
        payload->height != surface->height ||
        payload->pixel_format != LGP_PIXEL_FORMAT_XRGB8888 ||
        payload->stride < payload->width * LGP_BYTES_PER_PIXEL_XRGB8888 ||
        payload->height == 0 ||
        payload->stride > SIZE_MAX / payload->height) {
        close(buffer_fd);
        return LGP_SURFACE_STATUS_BAD_BUFFER;
    }

    size_t min_size = (size_t)payload->stride * (size_t)payload->height;
    if (payload->byte_size < min_size || payload->byte_size == 0) {
        close(buffer_fd);
        return LGP_SURFACE_STATUS_BAD_BUFFER;
    }

    void *map = mmap(NULL, payload->byte_size, PROT_READ, MAP_SHARED, buffer_fd, 0);
    int saved_errno = errno;
    close(buffer_fd);
    if (map == MAP_FAILED) {
        LGP_WARN("surface", "mmap() failed for surface id=%u: %s",
                 surface->id, strerror(saved_errno));
        return LGP_SURFACE_STATUS_BAD_BUFFER;
    }

    if (surface->buffer_map) {
        munmap(surface->buffer_map, surface->buffer_size);
    }
    surface->buffer_map = map;
    surface->buffer_size = payload->byte_size;
    surface->stride = payload->stride;
    surface->pixel_format = payload->pixel_format;

    LGP_DEBUG("surface", "Committed buffer surface id=%u session=%u size=%u stride=%u",
              surface->id, surface->owner_session_id, payload->byte_size, payload->stride);
    return LGP_SURFACE_STATUS_OK;
}

void lgp_surface_manager_destroy_for_client(lgp_surface_manager_t *manager,
                                            const lgp_client_t *client) {
    if (!manager || !client) return;

    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        lgp_surface_t *surface = &manager->surfaces[i];
        if (surface->in_use && surface->owner_session_id == client->session_id) {
            LGP_DEBUG("surface", "Client cleanup destroyed surface id=%u session=%u",
                      surface->id, surface->owner_session_id);
            lgp_surface_release(surface);
        }
    }
}

int lgp_surface_manager_composite(const lgp_surface_manager_t *manager,
                                  void *dst,
                                  uint32_t dst_width,
                                  uint32_t dst_height,
                                  uint32_t dst_pitch) {
    static const uint32_t layer_order[] = {
        LGP_LAYER_WALLPAPER,
        LGP_LAYER_APPLICATION,
        LGP_LAYER_SHELL,
        LGP_LAYER_OVERLAY,
        LGP_LAYER_NOTIFICATION,
        LGP_LAYER_LUNA_ISLAND,
        LGP_LAYER_SYSTEM_MODAL,
        LGP_LAYER_CURSOR
    };

    if (!manager || !dst || dst_width == 0 || dst_height == 0 ||
        dst_pitch < dst_width * LGP_BYTES_PER_PIXEL_XRGB8888) {
        return -EINVAL;
    }

    const uint32_t void_pixel = LGP_LUNA_VOID_XRGB8888;
    for (uint32_t y = 0; y < dst_height; y++) {
        uint8_t *row = (uint8_t *)dst + ((size_t)y * dst_pitch);
        for (uint32_t x = 0; x < dst_width; x++) {
            memcpy(row + ((size_t)x * LGP_BYTES_PER_PIXEL_XRGB8888),
                   &void_pixel,
                   sizeof(uint32_t));
        }
    }

    for (size_t layer_idx = 0; layer_idx < sizeof(layer_order) / sizeof(layer_order[0]); layer_idx++) {
        uint32_t layer = layer_order[layer_idx];
        for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
            const lgp_surface_t *surface = &manager->surfaces[i];
            if (!surface->in_use || surface->layer != layer || !surface->buffer_map) {
                continue;
            }

            for (uint32_t row_idx = 0; row_idx < surface->height; row_idx++) {
                const uint8_t *src_row = (const uint8_t *)surface->buffer_map +
                                         ((size_t)row_idx * surface->stride);
                uint8_t *dst_row = (uint8_t *)dst +
                                   ((size_t)((uint32_t)surface->y + row_idx) * dst_pitch) +
                                   ((size_t)(uint32_t)surface->x * LGP_BYTES_PER_PIXEL_XRGB8888);
                memcpy(dst_row, src_row, (size_t)surface->width * LGP_BYTES_PER_PIXEL_XRGB8888);
            }
        }
    }

    return 0;
}

const lgp_surface_t *lgp_surface_manager_find(const lgp_surface_manager_t *manager,
                                              uint32_t surface_id) {
    if (!manager) return NULL;

    for (size_t i = 0; i < LGP_SURFACE_MAX; i++) {
        if (manager->surfaces[i].in_use && manager->surfaces[i].id == surface_id) {
            return &manager->surfaces[i];
        }
    }
    return NULL;
}
