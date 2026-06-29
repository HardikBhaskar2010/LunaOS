/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_SCENE_SURFACE_H
#define MAHINA_LGP_SCENE_SURFACE_H

#include "../ipc/client.h"
#include "../protocol/surface.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LGP_SURFACE_MAX 64u

typedef struct {
    bool in_use;
    uint32_t id;
    uint32_t owner_session_id;
    uint32_t surface_type;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t layer;
    uint32_t stride;
    uint32_t pixel_format;
    size_t buffer_size;
    void *buffer_map;
} lgp_surface_t;

typedef struct {
    uint32_t next_surface_id;
    lgp_surface_t surfaces[LGP_SURFACE_MAX];
} lgp_surface_manager_t;

void lgp_surface_manager_init(lgp_surface_manager_t *manager);
uint32_t lgp_surface_manager_create(lgp_surface_manager_t *manager,
                                    const lgp_client_t *client,
                                    const lgp_surface_create_payload_t *payload,
                                    uint32_t display_width,
                                    uint32_t display_height,
                                    uint32_t *out_surface_id);
uint32_t lgp_surface_manager_destroy(lgp_surface_manager_t *manager,
                                     const lgp_client_t *client,
                                     uint32_t surface_id);
uint32_t lgp_surface_manager_commit(lgp_surface_manager_t *manager,
                                    const lgp_client_t *client,
                                    const lgp_surface_commit_payload_t *payload,
                                    int buffer_fd);
void lgp_surface_manager_destroy_for_client(lgp_surface_manager_t *manager,
                                            const lgp_client_t *client);
int lgp_surface_manager_composite(const lgp_surface_manager_t *manager,
                                  void *dst,
                                  uint32_t dst_width,
                                  uint32_t dst_height,
                                  uint32_t dst_pitch);
const lgp_surface_t *lgp_surface_manager_find(const lgp_surface_manager_t *manager,
                                              uint32_t surface_id);

#endif
