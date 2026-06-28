/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "hello.h"
#include "caps.h"
#include "../logging/log.h"
#include <unistd.h>

#define LGP_VERSION_MAJOR 1
#define LGP_VERSION_MINOR 0

void lgp_hello_handle(lgp_client_t *client, const lgp_msg_t *msg) {
    if (msg->length - LGP_HEADER_SIZE < 8) {
        LGP_ERROR("protocol", "LGP_HELLO payload too small");
        return;
    }

    const uint8_t *p = msg->payload;
    lgp_hello_payload_t hello = {
        .version_major = (uint16_t)(p[0] | (p[1] << 8)),
        .version_minor = (uint16_t)(p[2] | (p[3] << 8)),
        .caps_requested = (uint32_t)(p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24))
    };

    LGP_INFO("protocol", "Client (pid %d) sent LGP_HELLO v%u.%u", 
             (int)client->pid, hello.version_major, hello.version_minor);

    if (hello.version_major != LGP_VERSION_MAJOR) {
        LGP_WARN("protocol", "Client requested unsupported major version %u (we are %u)",
                 hello.version_major, LGP_VERSION_MAJOR);
        /* In a full implementation, send an ERROR message and drop connection */
        return;
    }

    /* Intersect requested capabilities with what this client's policy allows */
    uint32_t caps_granted = lgp_caps_negotiate(client, hello.caps_requested);

    /* Construct LGP_HELLO_REPLY */
    uint8_t reply[LGP_HEADER_SIZE + 8];
    lgp_tlv_encode_header(reply, sizeof(reply), LGP_MSG_HELLO_REPLY, sizeof(reply));
    
    reply[6] = (uint8_t)(LGP_VERSION_MAJOR & 0xFF);
    reply[7] = (uint8_t)((LGP_VERSION_MAJOR >> 8) & 0xFF);
    reply[8] = (uint8_t)(LGP_VERSION_MINOR & 0xFF);
    reply[9] = (uint8_t)((LGP_VERSION_MINOR >> 8) & 0xFF);
    reply[10] = (uint8_t)(caps_granted & 0xFF);
    reply[11] = (uint8_t)((caps_granted >> 8) & 0xFF);
    reply[12] = (uint8_t)((caps_granted >> 16) & 0xFF);
    reply[13] = (uint8_t)((caps_granted >> 24) & 0xFF);

    write(client->fd, reply, sizeof(reply));
}
