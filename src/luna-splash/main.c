/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "ipc.h"
#include "render.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/kd.h>

#define MAX_EVENTS 4

static int setup_signals(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        return -1;
    }
    
    return signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
}

int main(int argc, char **argv) {
    int pipe_fd = -1;
    int sigfd = -1;
    int tty_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    
    if (tty_fd >= 0) {
        ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fd") == 0 && i + 1 < argc) {
            pipe_fd = atoi(argv[i + 1]);
            /* Set the pipe to non-blocking so ipc_read_event doesn't hang when empty */
            int flags = fcntl(pipe_fd, F_GETFL, 0);
            if (flags >= 0) fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
            break;
        }
    }
    
    if (render_init() < 0) {
        fprintf(stderr, "luna-splash: Failed to initialize framebuffer\n");
        return 1;
    }
    dprintf(STDERR_FILENO, "luna-splash: init done, fb mapped\n");
    
    if (ipc_init(pipe_fd) < 0) {
        fprintf(stderr, "luna-splash: Warning: IPC not initialized\n");
    }
    
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) goto cleanup;
    
    sigfd = setup_signals();
    if (sigfd < 0) goto cleanup;
    
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = sigfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &ev);
    
    if (pipe_fd >= 0) {
        ev.events = EPOLLIN;
        ev.data.fd = pipe_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd, &ev);
    }
    
    /* Initial render */
    dprintf(STDERR_FILENO, "luna-splash: rendering initial logo\n");
    render_clear(COLOR_BG);
    render_logo();
    render_progress("Awaiting instructions...", 0);
    dprintf(STDERR_FILENO, "luna-splash: initial logo rendered\n");
    
    bool running = true;
    struct epoll_event events[MAX_EVENTS];
    
    while (running) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == sigfd) {
                struct signalfd_siginfo fdsi;
                ssize_t s = read(sigfd, &fdsi, sizeof(struct signalfd_siginfo));
                if (s == sizeof(struct signalfd_siginfo)) {
                    if (fdsi.ssi_signo == SIGTERM || fdsi.ssi_signo == SIGINT) {
                        running = false;
                    }
                }
            } else if (events[i].data.fd == pipe_fd) {
                /* Drain all complete messages buffered in this epoll event.
                 * ipc_read_event() returns one message per call; if two updates
                 * arrived in the same read(), the second was previously deferred
                 * until a third update arrived. (CODE_AUDIT_REPORT §5.2) */
                char msg[MAX_IPC_MSG];
                int percent;
                while (ipc_read_event(msg, &percent)) {
                    render_clear(COLOR_BG);
                    render_logo();
                    render_progress(msg, percent);
                }
            }
        }
    }
    
    dprintf(STDERR_FILENO, "luna-splash: exited epoll loop, fading out\n");
    render_fade_out();
    dprintf(STDERR_FILENO, "luna-splash: fade out complete, exiting\n");

cleanup:
    if (sigfd >= 0) close(sigfd);
    if (epfd >= 0) close(epfd);
    ipc_cleanup();
    render_cleanup();
    if (tty_fd >= 0) {
        ioctl(tty_fd, KDSETMODE, KD_TEXT);
        close(tty_fd);
    }
    return 0;
}
