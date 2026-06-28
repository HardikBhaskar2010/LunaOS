/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_DRM_CONNECTOR_H
#define MAHINA_LGP_DRM_CONNECTOR_H

#include "drm_internal.h"

/*
 * drm_connector_setup() — Find a suitable connector and select a preferred mode.
 * Populates dev->connector_id, dev->crtc_id, and dev->mode on success.
 * Returns 0 on success, negative on failure.
 */
int drm_connector_setup(drm_device_t *dev);

#endif
