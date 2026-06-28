/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "device.h"
#include "../logging/log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int drm_device_open(drm_device_t *dev, const char *path) {
    memset(dev, 0, sizeof(*dev));
    
    dev->fd = open(path, O_RDWR | O_CLOEXEC);
    if (dev->fd < 0) {
        LGP_ERROR("drm", "Failed to open DRM device %s: %s", path, strerror(errno));
        return -errno;
    }

    /* We implicitly acquire DRM master by opening the device, assuming luna-splash has closed it.
       If it fails, it means we don't have master, but we can try to explicitly acquire it if needed,
       or we can proceed and fail on mode set. We assume success for now per bring-up plan. */

    /* Check for atomic modesetting support */
    if (drmSetClientCap(dev->fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        dev->atomic_supported = true;
        LGP_INFO("drm", "Atomic modesetting supported on %s", path);
    } else {
        dev->atomic_supported = false;
        LGP_WARN("drm", "Atomic modesetting NOT supported on %s, fallback to legacy", path);
    }

    dev->resources = drmModeGetResources(dev->fd);
    if (!dev->resources) {
        LGP_ERROR("drm", "drmModeGetResources failed: %s", strerror(errno));
        close(dev->fd);
        dev->fd = -1;
        return -EIO;
    }

    LGP_INFO("drm", "Successfully opened DRM device %s", path);
    return 0;
}

void drm_device_close(drm_device_t *dev) {
    if (dev->resources) {
        drmModeFreeResources(dev->resources);
        dev->resources = NULL;
    }
    if (dev->fd >= 0) {
        /* Drop master if we had it, then close */
        drmDropMaster(dev->fd);
        close(dev->fd);
        dev->fd = -1;
    }
}
