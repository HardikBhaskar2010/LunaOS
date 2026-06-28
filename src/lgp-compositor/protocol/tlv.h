/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_PROTOCOL_TLV_H
#define MAHINA_LGP_PROTOCOL_TLV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * LGP Protocol Wire Format:
 * Every message is framed as:
 *   uint16_t type
 *   uint32_t length (includes header)
 *   uint8_t  payload[length - 6]
 */

#define LGP_HEADER_SIZE 6

#define LGP_MSG_HELLO       0x0001
#define LGP_MSG_HELLO_REPLY 0x0002
#define LGP_MSG_FILL_RECT   0x0010
#define LGP_MSG_ERROR       0xFFFF

typedef struct {
    uint16_t type;
    uint32_t length;
    const uint8_t *payload;
} lgp_msg_t;

/*
 * lgp_tlv_decode() — Decode a buffer into a message struct.
 * Checks bounds to ensure the buffer is at least `length` bytes long.
 * Returns true if a full message was decoded, false otherwise (e.g. partial).
 */
bool lgp_tlv_decode(const uint8_t *buf, size_t buf_len, lgp_msg_t *out_msg);

/*
 * lgp_tlv_encode_header() — Write type and length into a buffer.
 * Returns true if successful, false if buffer is too small.
 */
bool lgp_tlv_encode_header(uint8_t *buf, size_t buf_len, uint16_t type, uint32_t length);

#endif
