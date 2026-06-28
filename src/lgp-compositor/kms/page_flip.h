/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_KMS_PAGE_FLIP_H
#define MAHINA_LGP_KMS_PAGE_FLIP_H

#include "../drm/drm_internal.h"
#include "dumb_buffer.h"

/*
 * kms_page_flip() — Schedule a page flip for the CRTC.
 * Returns 0 on success, negative on failure.
 */
int kms_page_flip(drm_device_t *dev, dumb_buffer_t *fb, void *user_data);

/*
 * kms_handle_events() — Read and process DRM events (e.g., page flip completion).
 */
void kms_handle_events(drm_device_t *dev);

#endif
