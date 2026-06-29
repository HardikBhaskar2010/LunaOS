/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_PROTOCOL_SURFACE_H
#define MAHINA_LGP_PROTOCOL_SURFACE_H

#include "tlv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LGP_PIXEL_FORMAT_XRGB8888 0x34325258u /* DRM_FORMAT_XRGB8888: 'XR24' */

#define LGP_SURFACE_SYSTEM_CHROME       1u
#define LGP_SURFACE_LUNA_ISLAND         2u
#define LGP_SURFACE_SHELL_SURFACE       3u
#define LGP_SURFACE_APPLICATION_WINDOW  4u
#define LGP_SURFACE_CANVAS_SURFACE      5u
#define LGP_SURFACE_OVERLAY_SURFACE     6u

#define LGP_LAYER_WALLPAPER      0u
#define LGP_LAYER_APPLICATION    100u
#define LGP_LAYER_SHELL          200u
#define LGP_LAYER_OVERLAY        300u
#define LGP_LAYER_NOTIFICATION   400u
#define LGP_LAYER_LUNA_ISLAND    500u
#define LGP_LAYER_SYSTEM_MODAL   600u
#define LGP_LAYER_CURSOR         700u

#define LGP_SURFACE_STATUS_OK          0u
#define LGP_SURFACE_STATUS_INVALID     1u
#define LGP_SURFACE_STATUS_DENIED      2u
#define LGP_SURFACE_STATUS_NO_MEMORY   3u
#define LGP_SURFACE_STATUS_NOT_FOUND   4u
#define LGP_SURFACE_STATUS_BAD_BUFFER  5u

typedef struct {
    uint32_t surface_type;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t layer;
} lgp_surface_create_payload_t;

typedef struct {
    uint32_t surface_id;
} lgp_surface_destroy_payload_t;

typedef struct {
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixel_format;
    uint32_t byte_size;
} lgp_surface_commit_payload_t;

bool lgp_surface_decode_create(const lgp_msg_t *msg, lgp_surface_create_payload_t *out_payload);
bool lgp_surface_decode_destroy(const lgp_msg_t *msg, lgp_surface_destroy_payload_t *out_payload);
bool lgp_surface_decode_commit(const lgp_msg_t *msg, lgp_surface_commit_payload_t *out_payload);
bool lgp_surface_encode_create_reply(uint8_t *buf, size_t buf_len, uint32_t status, uint32_t surface_id);

#endif
