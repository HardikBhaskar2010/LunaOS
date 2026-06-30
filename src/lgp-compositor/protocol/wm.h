/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_PROTOCOL_WM_H
#define MAHINA_LGP_PROTOCOL_WM_H

#include "tlv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LGP_WM_STATE_NORMAL     0u
#define LGP_WM_STATE_MAXIMIZED  1u
#define LGP_WM_STATE_MINIMIZED  2u
#define LGP_WM_STATE_FULLSCREEN 3u
#define LGP_WM_STATE_HIDDEN     4u

typedef struct {
    uint32_t surface_id;
    uint32_t surface_type;
    uint32_t width;
    uint32_t height;
} lgp_wm_surface_created_payload_t;

typedef struct {
    uint32_t surface_id;
} lgp_wm_surface_destroyed_payload_t;

typedef struct {
    uint32_t surface_id;
    int32_t x;
    int32_t y;
} lgp_wm_set_position_payload_t;

typedef struct {
    uint32_t session_id;
} lgp_wm_set_focus_payload_t;

typedef struct {
    uint32_t surface_id;
    uint32_t state;
} lgp_wm_set_state_payload_t;

typedef struct {
    uint32_t key;
    uint32_t modifiers;
} lgp_wm_grab_key_payload_t;

bool lgp_wm_encode_surface_created(uint8_t *buf, size_t buf_len, uint32_t surface_id, uint32_t type, uint32_t w, uint32_t h);
bool lgp_wm_encode_surface_destroyed(uint8_t *buf, size_t buf_len, uint32_t surface_id);

bool lgp_wm_decode_set_position(const lgp_msg_t *msg, lgp_wm_set_position_payload_t *out);
bool lgp_wm_decode_set_focus(const lgp_msg_t *msg, lgp_wm_set_focus_payload_t *out);
bool lgp_wm_decode_set_state(const lgp_msg_t *msg, lgp_wm_set_state_payload_t *out);
bool lgp_wm_decode_grab_key(const lgp_msg_t *msg, lgp_wm_grab_key_payload_t *out);

#endif
