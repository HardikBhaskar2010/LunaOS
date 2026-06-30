/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "wm.h"
#include <string.h>

static void write_u32_le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static uint32_t read_u32_le(const uint8_t *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

bool lgp_wm_encode_surface_created(uint8_t *buf, size_t buf_len, uint32_t surface_id, uint32_t type, uint32_t w, uint32_t h) {
    if (buf_len < LGP_HEADER_SIZE + 16) return false;
    lgp_tlv_encode_header(buf, buf_len, LGP_MSG_WM_SURFACE_CREATED, LGP_HEADER_SIZE + 16);
    write_u32_le(buf + LGP_HEADER_SIZE, surface_id);
    write_u32_le(buf + LGP_HEADER_SIZE + 4, type);
    write_u32_le(buf + LGP_HEADER_SIZE + 8, w);
    write_u32_le(buf + LGP_HEADER_SIZE + 12, h);
    return true;
}

bool lgp_wm_encode_surface_destroyed(uint8_t *buf, size_t buf_len, uint32_t surface_id) {
    if (buf_len < LGP_HEADER_SIZE + 4) return false;
    lgp_tlv_encode_header(buf, buf_len, LGP_MSG_WM_SURFACE_DESTROYED, LGP_HEADER_SIZE + 4);
    write_u32_le(buf + LGP_HEADER_SIZE, surface_id);
    return true;
}

bool lgp_wm_decode_set_position(const lgp_msg_t *msg, lgp_wm_set_position_payload_t *out) {
    if (msg->type != LGP_MSG_WM_SET_SURFACE_POSITION || msg->length < LGP_HEADER_SIZE + 12) return false;
    out->surface_id = read_u32_le(msg->payload + 0);
    out->x = (int32_t)read_u32_le(msg->payload + 4);
    out->y = (int32_t)read_u32_le(msg->payload + 8);
    return true;
}

bool lgp_wm_decode_set_focus(const lgp_msg_t *msg, lgp_wm_set_focus_payload_t *out) {
    if (msg->type != LGP_MSG_WM_SET_FOCUS || msg->length < LGP_HEADER_SIZE + 4) return false;
    out->session_id = read_u32_le(msg->payload + 0);
    return true;
}

bool lgp_wm_decode_set_state(const lgp_msg_t *msg, lgp_wm_set_state_payload_t *out) {
    if (msg->type != LGP_MSG_WM_SET_STATE || msg->length < LGP_HEADER_SIZE + 8) return false;
    out->surface_id = read_u32_le(msg->payload + 0);
    out->state = read_u32_le(msg->payload + 4);
    return true;
}

bool lgp_wm_decode_grab_key(const lgp_msg_t *msg, lgp_wm_grab_key_payload_t *out) {
    if (msg->type != LGP_MSG_WM_GRAB_KEY || msg->length < LGP_HEADER_SIZE + 8) return false;
    out->key = read_u32_le(msg->payload + 0);
    out->modifiers = read_u32_le(msg->payload + 4);
    return true;
}
