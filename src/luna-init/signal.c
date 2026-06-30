/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * signal.c — Signal Handler Implementation
 * Authority: Volume II / 04_init_system.md §Signal Handling in luna-init
 */

#include "signal.h"
#include "log.h"

#include <errno.h>
#include <signal.h> // NOLINT(readability-duplicate-include)

#include <string.h>
#include <unistd.h>

#define COMP "signal"

int signal_init(void) {
    sigset_t mask;
    sigemptyset(&mask);

    /* Block all signals we handle via signalfd */
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGUSR1);

    /* Block the signal mask process-wide — signalfd will receive them */
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        LUNA_FATAL(COMP, "sigprocmask failed: %s", strerror(errno));
        return -1;
    }

    /* Create signalfd — events will appear on the returned fd */
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sfd < 0) {
        LUNA_FATAL(COMP, "signalfd failed: %s", strerror(errno));
        return -1;
    }

    LUNA_INFO(COMP, "Signal handler initialized (signalfd=%d)", sfd);
    return sfd;
}

signal_action_t signal_read(int sigfd) {
    struct signalfd_siginfo info;
    ssize_t n = read(sigfd, &info, sizeof(info));

    if (n != sizeof(info)) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LUNA_ERROR(COMP, "signalfd read error: %s", strerror(errno));
        }
        return SIGNAL_ACTION_NONE;
    }

    switch ((int)info.ssi_signo) {
        case SIGCHLD:
            LUNA_DEBUG(COMP, "SIGCHLD received (PID %u exited, status %d)",
                       info.ssi_pid, info.ssi_status);
            return SIGNAL_ACTION_REAP;

        case SIGTERM:
            LUNA_INFO(COMP, "SIGTERM received — initiating orderly shutdown");
            luna_log_signal_safe("SIGTERM received");
            return SIGNAL_ACTION_SHUTDOWN;

        case SIGINT:
            LUNA_INFO(COMP, "SIGINT received — initiating orderly reboot");
            luna_log_signal_safe("SIGINT received");
            return SIGNAL_ACTION_REBOOT;

        case SIGHUP:
            LUNA_INFO(COMP, "SIGHUP received — reloading service definitions");
            return SIGNAL_ACTION_RELOAD;

        case SIGUSR1:
            LUNA_INFO(COMP, "SIGUSR1 received — dumping state");
            return SIGNAL_ACTION_DUMP;

        default:
            LUNA_WARN(COMP, "Unexpected signal %u received", info.ssi_signo);
            return SIGNAL_ACTION_NONE;
    }
}

void signal_close(int sigfd) {
    if (sigfd >= 0) close(sigfd);
}
