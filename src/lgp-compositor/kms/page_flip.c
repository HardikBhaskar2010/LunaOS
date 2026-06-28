/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "page_flip.h"
#include "../logging/log.h"
#include <errno.h>
#include <string.h>

static void page_flip_handler(int fd __attribute__((unused)), unsigned int frame __attribute__((unused)),
                              unsigned int sec __attribute__((unused)), unsigned int usec __attribute__((unused)),
                              void *data) {
    /* M1: Just log that it happened for debugging. In later stages, this drives the animation loop. */
    LGP_DEBUG("kms", "Page flip completed, data=%p", data);
}

int kms_page_flip(drm_device_t *dev, dumb_buffer_t *fb, void *user_data) {
    if (!dev || !fb) return -EINVAL;

    int ret = drmModePageFlip(dev->fd, dev->crtc_id, fb->fb_id, DRM_MODE_PAGE_FLIP_EVENT, user_data);
    if (ret < 0) {
        LGP_ERROR("kms", "drmModePageFlip failed: %s", strerror(errno));
        return ret;
    }

    return 0;
}

void kms_handle_events(drm_device_t *dev) {
    drmEventContext ev_ctx = {0};
    ev_ctx.version = 2;
    ev_ctx.vblank_handler = NULL;
    ev_ctx.page_flip_handler = page_flip_handler;
    
    drmHandleEvent(dev->fd, &ev_ctx);
}
