/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * console.c — Console Output and Welcome Message
 * Authority: Volume VII / 01_implementation_roadmap.md §Stage 0 Success Criteria
 */

#include "console.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define COMP "console"

/* The Stage 0 success criterion message — exact text per mission brief */
static const char WELCOME_BANNER[] =
    "\r\n"
    "\r\n"
    "\r\n"
    "\033[1;37m"   /* Bold white */
    "  Welcome to Mahina.\r\n"
    "\033[0;37m"   /* Normal white */
    "  Architecture Freeze v1\r\n"
    "  System Initialization Complete.\r\n"
    "\033[0m"
    "\r\n"
    "  Stage 0 (v0.1 Bring-up) — Boot chain verified.\r\n"
    "  Type 'luna-init-ctl list' to see service states.\r\n"
    "\r\n";

void console_print_welcome(void) {
    (void)write(STDOUT_FILENO, WELCOME_BANNER, sizeof(WELCOME_BANNER) - 1);
    LUNA_INFO(COMP, "Welcome message displayed");
}

void console_drop_to_shell(const char *tty_path) {
    LUNA_INFO(COMP, "Dropping to interactive root shell on %s", tty_path ? tty_path : "default terminal");

    if (tty_path) {
        int fd = open(tty_path, O_RDWR);
        if (fd >= 0) {
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
            
            setsid();
            ioctl(0, TIOCSCTTY, 1);
        } else {
            LUNA_WARN(COMP, "Failed to open tty %s: %s", tty_path, strerror(errno));
        }
    }

    /* In v0.1, drop to busybox sh. In v0.5+ this path is replaced by
     * the login manager (luna-lock). */
    const char *shell_paths[] = {
        "/bin/sh",
        "/usr/bin/sh",
        "/bin/busybox",
        NULL
    };

    const char *argv[] = { "sh", NULL };
    const char *envp[] = {
        "PATH=/usr/bin:/usr/sbin:/bin:/sbin",
        "TERM=linux",
        "HOME=/root",
        "USER=root",
        "MAHINA_VERSION=0.1.0",
        NULL
    };

    for (int i = 0; shell_paths[i]; i++) {
        argv[0] = shell_paths[i];
        execve(shell_paths[i], (char * const *)argv, (char * const *)envp);
    }

    LUNA_FATAL(COMP, "No shell found — system halted");
    while (1) { pause(); }
}
