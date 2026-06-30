/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "surface.h"

#define LGP_CREATE_SURFACE_PAYLOAD_SIZE 24u
#define LGP_DESTROY_SURFACE_PAYLOAD_SIZE 4u
#define LGP_COMMIT_BUFFER_PAYLOAD_SIZE 24u
#define LGP_CREATE_SURFACE_REPLY_SIZE (LGP_HEADER_SIZE + 8u)

static uint32_t lgp_read_u32_le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t lgp_read_i32_le(const uint8_t *p) {
    return (int32_t)lgp_read_u32_le(p);
}

static void lgp_write_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
    p[2] = (uint8_t)((value >> 16) & 0xFFU);
    p[3] = (uint8_t)((value >> 24) & 0xFFU);
}

bool lgp_surface_decode_create(const lgp_msg_t *msg, lgp_surface_create_payload_t *out_payload) {
    if (!msg || !out_payload || msg->type != LGP_MSG_CREATE_SURFACE) {
        return false;
    }

    size_t payload_len = msg->length - LGP_HEADER_SIZE;
    if (payload_len != LGP_CREATE_SURFACE_PAYLOAD_SIZE) {
        return false;
    }

    const uint8_t *p = msg->payload;
    out_payload->surface_type = lgp_read_u32_le(p);
    out_payload->x = lgp_read_i32_le(p + 4);
    out_payload->y = lgp_read_i32_le(p + 8);
    out_payload->width = lgp_read_u32_le(p + 12);
    out_payload->height = lgp_read_u32_le(p + 16);
    out_payload->layer = lgp_read_u32_le(p + 20);
    return true;
}

bool lgp_surface_decode_destroy(const lgp_msg_t *msg, lgp_surface_destroy_payload_t *out_payload) {
    if (!msg || !out_payload || msg->type != LGP_MSG_DESTROY_SURFACE) {
        return false;
    }

    size_t payload_len = msg->length - LGP_HEADER_SIZE;
    if (payload_len != LGP_DESTROY_SURFACE_PAYLOAD_SIZE) {
        return false;
    }

    out_payload->surface_id = lgp_read_u32_le(msg->payload);
    return true;
}

bool lgp_surface_decode_commit(const lgp_msg_t *msg, lgp_surface_commit_payload_t *out_payload) {
    if (!msg || !out_payload || msg->type != LGP_MSG_COMMIT_BUFFER) {
        return false;
    }

    size_t payload_len = msg->length - LGP_HEADER_SIZE;
    if (payload_len != LGP_COMMIT_BUFFER_PAYLOAD_SIZE) {
        return false;
    }

    const uint8_t *p = msg->payload;
    out_payload->surface_id = lgp_read_u32_le(p);
    out_payload->width = lgp_read_u32_le(p + 4);
    out_payload->height = lgp_read_u32_le(p + 8);
    out_payload->stride = lgp_read_u32_le(p + 12);
    out_payload->pixel_format = lgp_read_u32_le(p + 16);
    out_payload->byte_size = lgp_read_u32_le(p + 20);
    return true;
}

bool lgp_surface_encode_create_reply(uint8_t *buf, size_t buf_len, uint32_t status, uint32_t surface_id) {
    if (!buf || buf_len < LGP_CREATE_SURFACE_REPLY_SIZE) {
        return false;
    }

    if (!lgp_tlv_encode_header(buf, buf_len, LGP_MSG_CREATE_SURFACE_REPLY, LGP_CREATE_SURFACE_REPLY_SIZE)) {
        return false;
    }

    lgp_write_u32_le(buf + LGP_HEADER_SIZE, status);
    lgp_write_u32_le(buf + LGP_HEADER_SIZE + 4, surface_id);
    return true;
}
