/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_DRM_INTERNAL_H
#define MAHINA_LGP_DRM_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Forward declaration for KMS buffer */
typedef struct dumb_buffer dumb_buffer_t;

typedef struct {
    int fd;
    bool atomic_supported;
    drmModeRes *resources;
    
    /* The single output we are driving for now (Stage 1) */
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
    
    /* The active framebuffer */
    dumb_buffer_t *front_buffer;
} drm_device_t;

#endif
