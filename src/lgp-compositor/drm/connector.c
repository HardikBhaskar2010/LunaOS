/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "connector.h"
#include "../logging/log.h"
#include <errno.h>

static int find_crtc_for_encoder(drm_device_t *dev, drmModeEncoder *enc) {
    if (enc->crtc_id) {
        return enc->crtc_id;
    }
    
    /* Fallback: find any available CRTC for this encoder */
    for (int i = 0; i < dev->resources->count_crtcs; i++) {
        if (enc->possible_crtcs & (1 << i)) {
            return dev->resources->crtcs[i];
        }
    }
    return -1;
}

static int find_crtc_for_connector(drm_device_t *dev, drmModeConnector *conn) {
    if (conn->encoder_id) {
        drmModeEncoder *enc = drmModeGetEncoder(dev->fd, conn->encoder_id);
        if (enc) {
            int crtc = find_crtc_for_encoder(dev, enc);
            drmModeFreeEncoder(enc);
            if (crtc >= 0) return crtc;
        }
    }

    /* Fallback: try all encoders attached to this connector */
    for (int i = 0; i < conn->count_encoders; i++) {
        drmModeEncoder *enc = drmModeGetEncoder(dev->fd, conn->encoders[i]);
        if (enc) {
            int crtc = find_crtc_for_encoder(dev, enc);
            drmModeFreeEncoder(enc);
            if (crtc >= 0) return crtc;
        }
    }

    return -1;
}

int drm_connector_setup(drm_device_t *dev) {
    if (!dev->resources) return -EINVAL;

    for (int i = 0; i < dev->resources->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(dev->fd, dev->resources->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            /* Find a preferred mode, or just take the first one */
            drmModeModeInfo *mode = &conn->modes[0];
            for (int m = 0; m < conn->count_modes; m++) {
                if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                    mode = &conn->modes[m];
                    break;
                }
            }

            int crtc_id = find_crtc_for_connector(dev, conn);
            if (crtc_id >= 0) {
                dev->connector_id = conn->connector_id;
                dev->crtc_id = crtc_id;
                dev->mode = *mode;
                
                LGP_INFO("drm", "Selected connector %u, CRTC %u, mode %ux%u@%uHz",
                         dev->connector_id, dev->crtc_id, dev->mode.hdisplay, dev->mode.vdisplay, dev->mode.vrefresh);
                
                drmModeFreeConnector(conn);
                return 0;
            }
        }
        drmModeFreeConnector(conn);
    }

    LGP_ERROR("drm", "Failed to find a suitable connected connector with a valid mode and CRTC");
    return -ENODEV;
}
