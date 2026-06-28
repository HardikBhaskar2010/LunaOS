/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_KMS_DUMB_BUFFER_H
#define MAHINA_LGP_KMS_DUMB_BUFFER_H

#include "../drm/drm_internal.h"
#include <stdint.h>

struct dumb_buffer {
    uint32_t handle;
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t size;
    void *map;
};

/*
 * kms_dumb_buffer_create() — Create and map a dumb buffer for the given mode.
 * Returns a pointer to the buffer on success, NULL on failure.
 */
dumb_buffer_t *kms_dumb_buffer_create(drm_device_t *dev, uint32_t width, uint32_t height);

/*
 * kms_dumb_buffer_destroy() — Unmap and free a dumb buffer.
 */
void kms_dumb_buffer_destroy(drm_device_t *dev, dumb_buffer_t *buf);

#endif
