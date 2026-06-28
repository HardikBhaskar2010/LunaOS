/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_SIGNAL_H
#define MAHINA_LGP_SIGNAL_H

/*
 * lgp_signal.h — signalfd wrapper
 */

typedef enum {
    LGP_SIGNAL_NONE,
    LGP_SIGNAL_SHUTDOWN,
    LGP_SIGNAL_RELOAD
} lgp_signal_action_t;

/*
 * lgp_signal_init() — Block standard signals and create signalfd.
 * Returns the signalfd on success, -1 on failure.
 */
int lgp_signal_init(void);

/*
 * lgp_signal_read() — Read from signalfd and return the corresponding action.
 */
lgp_signal_action_t lgp_signal_read(int fd);

#endif
