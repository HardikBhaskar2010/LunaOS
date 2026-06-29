/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_PROTOCOL_HELLO_H
#define MAHINA_LGP_PROTOCOL_HELLO_H

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "../ipc/client.h"

/*
 * LGP_HELLO Payload:
 *   uint16_t version_major
 *   uint16_t version_minor
 *   uint32_t caps_requested
 */
typedef struct {
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t caps_requested;
} lgp_hello_payload_t;

/*
 * lgp_hello_handle() — Handle an LGP_HELLO message from a client.
 * Sends back an LGP_HELLO_REPLY if the version is acceptable.
 */
bool lgp_hello_handle(lgp_client_t *client, const lgp_msg_t *msg);

#endif
