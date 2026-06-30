/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

static int g_log_fd = -1;

int lgp_log_init(const char *runtime_log_path) {
    if (g_log_fd >= 0) return 0; /* Already initialized */

    g_log_fd = open(runtime_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (g_log_fd < 0) {
        return -errno;
    }
    return 0;
}

void lgp_log_close(void) {
    if (g_log_fd >= 0) {
        close(g_log_fd);
        g_log_fd = -1;
    }
}

void lgp_log(lgp_log_level_t level, const char *component, const char *fmt, ...) {
    if (g_log_fd < 0) return;

    /* Get current time */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *t_info = gmtime(&tv.tv_sec);

    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             t_info->tm_year + 1900, t_info->tm_mon + 1, t_info->tm_mday,
             t_info->tm_hour, t_info->tm_min, t_info->tm_sec,
             (int)(tv.tv_usec / 1000));

    const char *lvl_str;
    switch (level) {
        case LOG_FATAL: lvl_str = "FATAL"; break;
        case LOG_ERROR: lvl_str = "ERROR"; break;
        case LOG_WARN:  lvl_str = "WARN "; break;
        case LOG_INFO:  lvl_str = "INFO "; break;
        case LOG_DEBUG: lvl_str = "DEBUG"; break;
        case LOG_TRACE: lvl_str = "TRACE"; break;
        default:        lvl_str = "UNK  "; break;
    }

    char buf[4096];
    int len = snprintf(buf, sizeof(buf), "[%s] [%s] [%s] ", time_str, lvl_str, component);
    
    if (len > 0 && len < (int)sizeof(buf)) {
        va_list args;
        va_start(args, fmt);
        int msg_len = vsnprintf(buf + len, sizeof(buf) - len, fmt, args); // NOLINT(clang-analyzer-valist.Uninitialized)
        va_end(args);

        if (msg_len > 0) {
            len += msg_len;
            if (len >= (int)sizeof(buf)) {
                len = sizeof(buf) - 2;
            }
            buf[len++] = '\n';
            buf[len] = '\0';
            write(g_log_fd, buf, len);
        }
    }

    if (level == LOG_FATAL) {
        fsync(g_log_fd);
    }
}
