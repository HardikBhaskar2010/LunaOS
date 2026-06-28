/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "signal.h"
#include "../logging/log.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

int lgp_signal_init(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);

    /* Block signals so they aren't handled by the default handlers */
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        LGP_FATAL("signal", "sigprocmask failed: %s", strerror(errno));
        return -1;
    }

    int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd == -1) {
        LGP_FATAL("signal", "signalfd failed: %s", strerror(errno));
        return -1;
    }

    return fd;
}

lgp_signal_action_t lgp_signal_read(int fd) {
    struct signalfd_siginfo fdsi;
    ssize_t s = read(fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        if (s < 0 && (errno == EAGAIN || errno == EINTR)) {
            return LGP_SIGNAL_NONE;
        }
        LGP_ERROR("signal", "signalfd read failed: %s", strerror(errno));
        return LGP_SIGNAL_NONE;
    }

    switch (fdsi.ssi_signo) {
        case SIGTERM:
        case SIGINT:
            LGP_DEBUG("signal", "Received shutdown signal (%d)", fdsi.ssi_signo);
            return LGP_SIGNAL_SHUTDOWN;
        case SIGHUP:
            LGP_DEBUG("signal", "Received SIGHUP");
            return LGP_SIGNAL_RELOAD;
        default:
            return LGP_SIGNAL_NONE;
    }
}
