/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "crtc.h"
#include "../logging/log.h"
#include <errno.h>
#include <string.h>

int kms_crtc_commit(drm_device_t *dev, dumb_buffer_t *fb) {
    if (!dev || !fb) return -EINVAL;

    /* For M1, we use legacy drmModeSetCrtc. 
       A full atomic implementation requires fetching all property IDs for CRTC, 
       Connector, and Plane, which will be implemented in later stages. */
       
    int ret = drmModeSetCrtc(dev->fd, dev->crtc_id, fb->fb_id, 0, 0,
                             &dev->connector_id, 1, &dev->mode);
    if (ret < 0) {
        LGP_ERROR("kms", "drmModeSetCrtc failed: %s", strerror(errno));
        return ret;
    }

    LGP_INFO("kms", "CRTC %u committed successfully", dev->crtc_id);
    return 0;
}
