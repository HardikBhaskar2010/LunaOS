/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "dumb_buffer.h"
#include "../logging/log.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <drm_fourcc.h>

dumb_buffer_t *kms_dumb_buffer_create(drm_device_t *dev, uint32_t width, uint32_t height) {
    dumb_buffer_t *buf = calloc(1, sizeof(*buf));
    if (!buf) {
        LGP_ERROR("kms", "Failed to allocate dumb buffer struct");
        return NULL;
    }

    struct drm_mode_create_dumb create_arg = {0};
    create_arg.width = width;
    create_arg.height = height;
    create_arg.bpp = 32;

    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg) < 0) {
        LGP_ERROR("kms", "DRM_IOCTL_MODE_CREATE_DUMB failed: %s", strerror(errno));
        free(buf);
        return NULL;
    }

    buf->handle = create_arg.handle;
    buf->width = width;
    buf->height = height;
    buf->pitch = create_arg.pitch;
    buf->size = create_arg.size;

    /* Add framebuffer for this dumb buffer */
    uint32_t handles[4] = { buf->handle };
    uint32_t pitches[4] = { buf->pitch };
    uint32_t offsets[4] = { 0 };

    if (drmModeAddFB2(dev->fd, width, height, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &buf->fb_id, 0) < 0) {
        LGP_ERROR("kms", "drmModeAddFB2 failed: %s", strerror(errno));
        /* Fallback to legacy AddFB if needed, but AddFB2 should work */
        if (drmModeAddFB(dev->fd, width, height, 24, 32, buf->pitch, buf->handle, &buf->fb_id) < 0) {
            LGP_ERROR("kms", "drmModeAddFB fallback failed: %s", strerror(errno));
            goto err_destroy;
        }
    }

    /* Map the buffer to userspace */
    struct drm_mode_map_dumb map_arg = {0};
    map_arg.handle = buf->handle;
    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg) < 0) {
        LGP_ERROR("kms", "DRM_IOCTL_MODE_MAP_DUMB failed: %s", strerror(errno));
        goto err_rmfb;
    }

    buf->map = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, map_arg.offset);
    if (buf->map == MAP_FAILED) {
        LGP_ERROR("kms", "mmap dumb buffer failed: %s", strerror(errno));
        goto err_rmfb;
    }

    return buf;

err_rmfb:
    drmModeRmFB(dev->fd, buf->fb_id);
err_destroy:
    {
        struct drm_mode_destroy_dumb destroy_arg = {0};
        destroy_arg.handle = buf->handle;
        drmIoctl(dev->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
    }
    free(buf);
    return NULL;
}

void kms_dumb_buffer_destroy(drm_device_t *dev, dumb_buffer_t *buf) {
    if (!buf) return;
    if (buf->map && buf->map != MAP_FAILED) {
        munmap(buf->map, buf->size);
    }
    if (buf->fb_id) {
        drmModeRmFB(dev->fd, buf->fb_id);
    }
    if (buf->handle) {
        struct drm_mode_destroy_dumb destroy_arg = {0};
        destroy_arg.handle = buf->handle;
        drmIoctl(dev->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
    }
    free(buf);
}
