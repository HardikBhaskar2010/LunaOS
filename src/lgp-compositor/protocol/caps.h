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

#define LGP_CAP_LAYER_SHELL    (1 << 0)
#define LGP_CAP_LUNA_ISLAND    (1 << 1)
#define LGP_CAP_SCREENCAPTURE  (1 << 2)

/*
 * lgp_caps_negotiate() — Determine granted capabilities based on requested caps
 * and the client's identity/policy.
 * Returns the granted capabilities bitmask.
 */
uint32_t lgp_caps_negotiate(lgp_client_t *client, uint32_t requested_caps);

#endif
