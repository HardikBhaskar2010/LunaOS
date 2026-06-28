/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_KMS_CRTC_H
#define MAHINA_LGP_KMS_CRTC_H

#include "../drm/drm_internal.h"
#include "dumb_buffer.h"

/*
 * kms_crtc_commit() — Perform a mode set and attach the framebuffer to the CRTC.
 * Returns 0 on success, negative on failure.
 */
int kms_crtc_commit(drm_device_t *dev, dumb_buffer_t *fb);

#endif
