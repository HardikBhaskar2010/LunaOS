/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_LOG_H
#define MAHINA_LGP_LOG_H

#include <stdarg.h>
#include <stdbool.h>

typedef enum {
    LOG_FATAL = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4,
    LOG_TRACE = 5,
} lgp_log_level_t;

/*
 * lgp_log_init() — Open runtime.log and record start timestamp.
 * Returns 0 on success, -errno on failure.
 */
int lgp_log_init(const char *runtime_log_path);

/*
 * lgp_log() — Write a log entry to runtime.log.
 * Format matches luna-init: [YYYY-MM-DDTHH:MM:SS.mmmZ] [LEVEL] [COMPONENT] message
 */
void lgp_log(lgp_log_level_t level, const char *component, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/*
 * lgp_log_close() — Close the log file descriptor.
 */
void lgp_log_close(void);

#define LGP_FATAL(comp, ...) do { lgp_log(LOG_FATAL, comp, __VA_ARGS__); } while(0)
#define LGP_ERROR(comp, ...) do { lgp_log(LOG_ERROR, comp, __VA_ARGS__); } while(0)
#define LGP_WARN(comp, ...)  do { lgp_log(LOG_WARN, comp, __VA_ARGS__);  } while(0)
#define LGP_INFO(comp, ...)  do { lgp_log(LOG_INFO, comp, __VA_ARGS__);  } while(0)
#define LGP_DEBUG(comp, ...) do { lgp_log(LOG_DEBUG, comp, __VA_ARGS__); } while(0)

#endif
