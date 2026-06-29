/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "caps.h"
#include "../logging/log.h"

uint32_t lgp_caps_negotiate(lgp_client_t *client, uint32_t requested_caps) {
    uint32_t granted = 0;

    if (requested_caps & LGP_CAP_CANVAS_SURFACE) {
        granted |= LGP_CAP_CANVAS_SURFACE;
    }
    if (requested_caps & LGP_CAP_DIRECT_LGP) {
        granted |= LGP_CAP_DIRECT_LGP;
    }

    /* Privileged compositor layers are still restricted to trusted system clients. */
    if (client->uid == 0) {
        if (requested_caps & LGP_CAP_LAYER_SHELL) {
            granted |= LGP_CAP_LAYER_SHELL;
        }
        if (requested_caps & LGP_CAP_LUNA_ISLAND) {
            granted |= LGP_CAP_LUNA_ISLAND;
        }
    }

    /* DMA-BUF, cursor shape, and clipboard are not implemented yet, so deny them. */

    if (requested_caps != granted) {
        LGP_WARN("protocol", "Client (pid %d) requested caps 0x%08x, but granted 0x%08x",
                 (int)client->pid, requested_caps, granted);
    }

    return granted;
}
