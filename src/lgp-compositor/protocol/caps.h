/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_PROTOCOL_CAPS_H
#define MAHINA_LGP_PROTOCOL_CAPS_H

#include <stdint.h>
#include "../ipc/client.h"

#define LGP_CAP_DMA_BUF        (1u << 0)
#define LGP_CAP_CANVAS_SURFACE (1u << 1)
#define LGP_CAP_DIRECT_LGP     (1u << 2)
#define LGP_CAP_LAYER_SHELL    (1u << 3)
#define LGP_CAP_LUNA_ISLAND    (1u << 4)
#define LGP_CAP_CURSOR_SHAPE   (1u << 5)
#define LGP_CAP_CLIPBOARD      (1u << 6)
#define LGP_CAP_WINDOW_MANAGER (1u << 7)

/*
 * lgp_caps_negotiate() — Determine granted capabilities based on requested caps
 * and the client's identity/policy.
 * Returns the granted capabilities bitmask.
 */
uint32_t lgp_caps_negotiate(lgp_client_t *client, uint32_t requested_caps);

#endif
