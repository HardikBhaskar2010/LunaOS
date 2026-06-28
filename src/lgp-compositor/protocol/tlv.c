/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "tlv.h"

bool lgp_tlv_decode(const uint8_t *buf, size_t buf_len, lgp_msg_t *out_msg) {
    if (buf_len < LGP_HEADER_SIZE) return false;

    /* Little-endian decoding */
    out_msg->type = (uint16_t)(buf[0] | (buf[1] << 8));
    out_msg->length = (uint32_t)(buf[2] | (buf[3] << 8) | (buf[4] << 16) | (buf[5] << 24));

    if (out_msg->length < LGP_HEADER_SIZE) return false;
    if (buf_len < out_msg->length) return false;

    out_msg->payload = buf + LGP_HEADER_SIZE;
    return true;
}

bool lgp_tlv_encode_header(uint8_t *buf, size_t buf_len, uint16_t type, uint32_t length) {
    if (buf_len < LGP_HEADER_SIZE) return false;
    
    if (length < LGP_HEADER_SIZE) length = LGP_HEADER_SIZE;

    /* Little-endian encoding */
    buf[0] = (uint8_t)(type & 0xFF);
    buf[1] = (uint8_t)((type >> 8) & 0xFF);
    
    buf[2] = (uint8_t)(length & 0xFF);
    buf[3] = (uint8_t)((length >> 8) & 0xFF);
    buf[4] = (uint8_t)((length >> 16) & 0xFF);
    buf[5] = (uint8_t)((length >> 24) & 0xFF);

    return true;
}
