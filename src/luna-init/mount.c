/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * mount.c — Filesystem Mount Manager Implementation
 * Authority: Volume II / 02_boot_flow.md §Stage 2
 * Authority: Volume II / 09_filesystem.md
 */

#include "mount.h"
#include "log.h"
#include "toml.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#define COMP "mount"

/* Critical mount points — failure is fatal (02_boot_flow.md §Stage 2) */
static const char * const CRITICAL_MOUNTPOINTS[] = {
    "/proc", "/sys", "/dev", NULL
};

/* Tracked mount table for unmount_all() */
static struct {
    char mountpoint[256];
    bool active;
} g_mounts[MOUNT_MAX_ENTRIES];
static int g_mount_count = 0;

static bool is_critical(const char *mp) {
    for (int i = 0; CRITICAL_MOUNTPOINTS[i]; i++) {
        if (strcmp(CRITICAL_MOUNTPOINTS[i], mp) == 0) return true;
    }
    return false;
}

static void track_mount(const char *mp) {
    if (g_mount_count >= MOUNT_MAX_ENTRIES) return;
    strncpy(g_mounts[g_mount_count].mountpoint, mp, 255);
    g_mounts[g_mount_count].mountpoint[255] = '\0';
    g_mounts[g_mount_count].active = true;
    g_mount_count++;
}

/*
 * do_mount() — Perform a single mount(2) call with logging.
 * options_str: comma-separated mount options (e.g. "size=2G,noexec,nosuid")
 * Returns MOUNT_OK, MOUNT_ERR_FATAL (critical), or MOUNT_ERR_WARN.
 */
static mount_result_t do_mount(const char *device, const char *mountpoint,
                                const char *fstype, const char *options_str) {
    /* Ensure mountpoint directory exists */
    struct stat st = {0};
    if (stat(mountpoint, &st) != 0) {
        if (mkdir(mountpoint, 0755) != 0 && errno != EEXIST) {
            LUNA_ERROR(COMP, "Cannot create mountpoint %s: %s",
                       mountpoint, strerror(errno));
            return is_critical(mountpoint) ? MOUNT_ERR_FATAL : MOUNT_ERR_WARN;
        }
    }

    unsigned long flags = 0;
    char data_buf[512] = "";
    const char *data = NULL;

    if (options_str && strlen(options_str) > 0) {
        char opt_copy[512];
        strncpy(opt_copy, options_str, sizeof(opt_copy) - 1);
        opt_copy[sizeof(opt_copy) - 1] = '\0';

        char *token = strtok(opt_copy, ",");
        bool first = true;
        while (token) {
            if (strcmp(token, "ro") == 0) {
                flags |= MS_RDONLY;
            } else if (strcmp(token, "noexec") == 0) {
                flags |= MS_NOEXEC;
            } else if (strcmp(token, "nosuid") == 0) {
                flags |= MS_NOSUID;
            } else if (strcmp(token, "nodev") == 0) {
                flags |= MS_NODEV;
            } else if (strcmp(token, "relatime") == 0) {
                flags |= MS_RELATIME;
            } else {
                if (!first) {
                    strncat(data_buf, ",", sizeof(data_buf) - strlen(data_buf) - 1);
                }
                strncat(data_buf, token, sizeof(data_buf) - strlen(data_buf) - 1);
                first = false;
            }
            token = strtok(NULL, ",");
        }
        if (strlen(data_buf) > 0) {
            data = data_buf;
        }
    }

    if (mount(device, mountpoint, fstype, flags, data) != 0) {
        int err = errno;
        if (err == EBUSY) {
            LUNA_INFO(COMP, "Already mounted (EBUSY): %s at %s", device, mountpoint);
            track_mount(mountpoint);
            return MOUNT_OK;
        }
        if (is_critical(mountpoint)) {
            LUNA_FATAL(COMP, "FATAL: Failed to mount %s (%s) at %s: %s",
                       device, fstype, mountpoint, strerror(err));
            return MOUNT_ERR_FATAL;
        } else {
            LUNA_WARN(COMP, "Failed to mount %s at %s: %s (continuing)",
                      device, mountpoint, strerror(err));
            return MOUNT_ERR_WARN;
        }
    }

    LUNA_INFO(COMP, "Mounted: %s (%s) at %s", device, fstype, mountpoint);
    track_mount(mountpoint);
    return MOUNT_OK;
}

mount_result_t mount_early(void) {
    /*
     * Mount the three mandatory virtual filesystems before we can read
     * fstab.toml or write to the boot log on the real root.
     * devtmpfs auto-mounted by kernel (DEVTMPFS_MOUNT=y), but we mount
     * proc and sys explicitly here.
     */
    mount_result_t r;

    r = do_mount("proc",    "/proc", "proc",     "");
    if (r == MOUNT_ERR_FATAL) return r;

    r = do_mount("sysfs",   "/sys",  "sysfs",    "");
    if (r == MOUNT_ERR_FATAL) return r;

    /* /dev may already be mounted by kernel devtmpfs — remount is fine */
    r = do_mount("devtmpfs","/dev",  "devtmpfs", "");
    if (r == MOUNT_ERR_FATAL) return r;

    /* Mount cgroup2 for service isolation (not fatal if it fails in Stage 0) */
    do_mount("cgroup2", "/sys/fs/cgroup", "cgroup2", "");

    return MOUNT_OK;
}

mount_result_t mount_fstab(const char *fstab_path) {
    toml_error_t  terr;
    toml_doc_t   *doc = toml_load_file(fstab_path, &terr);

    if (!doc) {
        LUNA_FATAL(COMP, "Cannot parse %s: %s", fstab_path, toml_strerror(terr));
        return MOUNT_ERR_FATAL;
    }

    int count = toml_array_table_count(doc, "mount");
    if (count == 0) {
        LUNA_WARN(COMP, "No [[mount]] entries in %s", fstab_path);
        toml_free(doc);
        return MOUNT_OK;
    }

    mount_result_t overall = MOUNT_OK;

    for (int i = 0; i < count; i++) {
        toml_value_t v_device     = {0};
        toml_value_t v_mountpoint = {0};
        toml_value_t v_fstype     = {0};
        toml_value_t v_options    = {0};

        if (toml_array_table_get(doc, "mount", i, "device",     &v_device)     != TOML_OK ||
            toml_array_table_get(doc, "mount", i, "mountpoint", &v_mountpoint) != TOML_OK ||
            toml_array_table_get(doc, "mount", i, "fstype",     &v_fstype)     != TOML_OK) {
            LUNA_ERROR(COMP, "[[mount]] entry %d missing required fields", i);
            overall = MOUNT_ERR_WARN;
            continue;
        }

        /* Build options string from array (e.g. ["rw", "relatime"] → "rw,relatime") */
        char options_str[512] = "";
        if (toml_array_table_get(doc, "mount", i, "options", &v_options) == TOML_OK &&
            v_options.type == TOML_TYPE_ARRAY) {
            for (size_t j = 0; j < v_options.v.array.count; j++) {
                if (j > 0) strncat(options_str, ",", sizeof(options_str) - strlen(options_str) - 1);
                strncat(options_str, v_options.v.array.items[j],
                        sizeof(options_str) - strlen(options_str) - 1);
            }
        }

        mount_result_t r = do_mount(v_device.v.str, v_mountpoint.v.str,
                                    v_fstype.v.str,  options_str);
        if (r == MOUNT_ERR_FATAL) {
            toml_free(doc);
            return MOUNT_ERR_FATAL;
        }
        if (r == MOUNT_ERR_WARN) overall = MOUNT_ERR_WARN;
    }

    toml_free(doc);
    return overall;
}

void mount_unmount_all(void) {
    /* Unmount in reverse order (LIFO) */
    for (int i = g_mount_count - 1; i >= 0; i--) {
        if (!g_mounts[i].active) continue;
        if (umount2(g_mounts[i].mountpoint, MNT_DETACH) == 0) {
            LUNA_INFO(COMP, "Unmounted: %s", g_mounts[i].mountpoint);
        } else {
            LUNA_WARN(COMP, "Failed to unmount %s: %s",
                      g_mounts[i].mountpoint, strerror(errno));
        }
        g_mounts[i].active = false;
    }
}
