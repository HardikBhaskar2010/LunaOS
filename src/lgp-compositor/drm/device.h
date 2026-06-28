/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_DRM_DEVICE_H
#define MAHINA_LGP_DRM_DEVICE_H

#include "drm_internal.h"

/*
 * drm_device_open() — Open the DRM device and check basic capabilities.
 * Tries the primary path first, then fallback path.
 * Returns 0 on success, negative on failure.
 */
int drm_device_open(drm_device_t *dev, const char *path);

/*
 * drm_device_close() — Clean up DRM resources.
 */
void drm_device_close(drm_device_t *dev);

#endif
