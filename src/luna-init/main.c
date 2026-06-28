/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * main.c — luna-init Entry Point (PID 1)
 * Authority: Volume II / 04_init_system.md
 * Authority: Volume II / 02_boot_flow.md
 * Authority: Core Law I (Own Every Layer)
 *
 * Boot sequence:
 *   Stage 0: Kernel handoff → luna-init alive
 *   Stage 1: Signal handling, zombie reaper, epoll loop setup
 *   Stage 2: Filesystem mounts (/proc /sys /dev /tmp /run)
 *   Stage 3: Early hooks (hostname, clock, entropy)
 *   Stage 4: Service supervisor init — start all services
 *   Stage 5+: Skeletons only (graphics/shell/AI are v0.5+)
 *
 * Event loop: epoll-based, non-blocking.
 * Signals via: signalfd (race-free, as per 04_init_system.md).
 * Zombie reaping: on every SIGCHLD via reaper_reap_all().
 */

#include "console.h"
#include "ctl.h"
#include "depgraph.h"
#include "hostname.h"
#include "log.h"
#include "mount.h"
#include "panic.h"
#include "reaper.h"
#include "service.h"
#include "shutdown.h"
#include "signal.h"
#include "splash.h"
#include "supervisor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

/* ─── Configuration ──────────────────────────────────────────────────────── */

#define BOOT_LOG_PATH     "/var/log/luna-init/boot.log"
#define RUNTIME_LOG_PATH  "/var/log/luna-init/runtime.log"
#define FSTAB_PATH        "/etc/luna/fstab.toml"
#define HOSTNAME_PATH     "/etc/luna/hostname"
#define SERVICES_DIR      "/etc/luna/services"

/* epoll events capacity */
#define EPOLL_MAX_EVENTS  8

/* ─── Ensure log directory exists before opening log file ────────────────── */

static void ensure_log_dir(void) {
    /* /var/log must already exist on the real root filesystem.
     * Create /var/log/luna-init/ if missing. */
    struct stat st = {0};
    if (stat("/var/log", &st) != 0) {
        mkdir("/var/log", 0755);
    }
    if (stat("/var/log/luna-init", &st) != 0) {
        mkdir("/var/log/luna-init", 0755);
    }
}

/* ─── Main entry point ───────────────────────────────────────────────────── */

int main(void) {
    /*
     * We are PID 1. The kernel has handed us control.
     * From this point forward, every decision is ours.
     *
     * Do not call exit() — that would trigger atexit() handlers and
     * may not work correctly as PID 1. Use _exit() if needed.
     */

    /* ═══ STAGE 0: PID 1 Alive ════════════════════════════════════════════ */

    /* Absolute first thing: mount /proc so we can read /proc/self.
     * This is done before log init because /var/log may be on the real root
     * which is already mounted (real root pivot happens in initramfs).
     * If we're running post-pivot, /proc is still needed. */
    mount_early();

    ensure_log_dir();

    luna_log_init(BOOT_LOG_PATH, RUNTIME_LOG_PATH);
    luna_log_set_stage(LUNA_STAGE_FIRMWARE);

    LUNA_INFO("luna-init",
              "PID 1 alive. Mahina " MAHINA_CODENAME " " MAHINA_VERSION);
    LUNA_INFO("luna-init", "Boot log: " BOOT_LOG_PATH);

    /* ═══ STAGE 1: Signal handling and epoll setup ════════════════════════ */

    luna_log_set_stage(LUNA_STAGE_KERNEL);
    LUNA_INFO("luna-init", "Stage 1: Initializing event loop and signal handling");
    splash_start();

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        LUNA_FATAL("luna-init", "epoll_create1 failed: %s", strerror(errno));
        panic_drop_to_shell("epoll_create1 failed");
    }

    /* Initialize signalfd and add to epoll */
    int sigfd = signal_init();
    if (sigfd < 0) {
        panic_drop_to_shell("signal_init failed");
    }

    struct epoll_event ev = {0};
    ev.events   = EPOLLIN;
    ev.data.fd  = sigfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &ev);

    /* ═══ STAGE 2: Filesystem mounts ═════════════════════════════════════ */

    luna_log_set_stage(LUNA_STAGE_FILESYSTEM);
    LUNA_INFO("luna-init", "Stage 2: Mounting filesystems");
    splash_update("Mounting filesystems", 20);

    /* Early mounts already done above — now process fstab for /tmp /run etc. */
    mount_result_t mresult = mount_fstab(FSTAB_PATH);
    if (mresult == MOUNT_ERR_FATAL) {
        panic_drop_to_shell("Critical filesystem mount failed");
    }

    /* ═══ STAGE 3: Early hooks ════════════════════════════════════════════ */

    luna_log_set_stage(LUNA_STAGE_HOOKS);
    LUNA_INFO("luna-init", "Stage 3: Running early hooks");
    splash_update("Running early hooks", 40);

    hostname_set(HOSTNAME_PATH);

    /* Seed kernel entropy from /etc/machine-id if present (non-fatal) */
    {
        int rfd = open("/etc/machine-id", O_RDONLY | O_CLOEXEC);
        if (rfd >= 0) {
            char seed[37];
            ssize_t n = read(rfd, seed, sizeof(seed) - 1);
            close(rfd);
            if (n > 0) {
                seed[n] = '\0';
                int wfd = open("/dev/urandom", O_WRONLY | O_CLOEXEC);
                if (wfd >= 0) {
                    (void)write(wfd, seed, (size_t)n);
                    close(wfd);
                    LUNA_DEBUG("luna-init", "Entropy seeded from machine-id");
                }
            }
        }
    }

    LUNA_INFO("luna-init", "Stage 3: Complete");

    /* ═══ STAGE 4: Service supervisor ════════════════════════════════════ */

    luna_log_set_stage(LUNA_STAGE_SERVICES);
    LUNA_INFO("luna-init", "Stage 4: Loading service definitions");
    splash_update("Loading service definitions", 60);

    int svc_count = service_load_all(SERVICES_DIR);
    if (svc_count < 0) {
        LUNA_WARN("luna-init",
                  "No services directory at %s — continuing without services",
                  SERVICES_DIR);
    } else if (svc_count > 0) {
        if (depgraph_build() < 0) {
            panic_drop_to_shell("Service dependency graph has a cycle");
        }
        LUNA_INFO("luna-init", "Stage 4: Starting %d services", svc_count);
        splash_update("Starting services", 80);
        supervisor_start_all();
    }

    /* ═══ Control socket ══════════════════════════════════════════════════ */

    int ctl_fd = ctl_server_init();
    if (ctl_fd >= 0) {
        ev.events  = EPOLLIN;
        ev.data.fd = ctl_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, ctl_fd, &ev);
    }

    int inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (inotify_fd >= 0) {
        inotify_add_watch(inotify_fd, SERVICES_DIR, 
                          IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        ev.events  = EPOLLIN;
        ev.data.fd = inotify_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, inotify_fd, &ev);
    }

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd >= 0) {
        struct itimerspec ts = {
            .it_interval = {0, 100000000}, /* 100ms */
            .it_value    = {0, 100000000}
        };
        timerfd_settime(timer_fd, 0, &ts, NULL);
        ev.events  = EPOLLIN;
        ev.data.fd = timer_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, &ev);
    }

    /* ═══ Main event loop ═════════════════════════════════════════════════ */

    LUNA_INFO("luna-init", "Entering main event loop");

    struct epoll_event events[EPOLL_MAX_EVENTS];
    bool boot_complete = false;

    while (!g_shutting_down) {
        int nfds = epoll_wait(epfd, events, EPOLL_MAX_EVENTS, -1);

        if (nfds < 0) {
            if (errno == EINTR) continue; /* Interrupted — retry */
            LUNA_ERROR("luna-init", "epoll_wait error: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == sigfd) {
                /* Signal received */
                signal_action_t action = signal_read(sigfd);
                switch (action) {
                    case SIGNAL_ACTION_REAP:
                        reaper_reap_all();
                        break;

                    case SIGNAL_ACTION_SHUTDOWN:
                        shutdown_run(SHUTDOWN_POWEROFF);
                        break; /* not reached */

                    case SIGNAL_ACTION_REBOOT:
                        shutdown_run(SHUTDOWN_REBOOT);
                        break; /* not reached */

                    case SIGNAL_ACTION_RELOAD:
                        LUNA_INFO("luna-init", "Reloading service definitions");
                        service_load_all(SERVICES_DIR);
                        depgraph_build();
                        break;

                    case SIGNAL_ACTION_DUMP: {
                        /* Dump service states to log */
                        LUNA_INFO("luna-init", "State dump requested:");
                        for (int j = 0; j < g_service_count; j++) {
                            LUNA_INFO("luna-init",
                                      "  %-20s %s (PID %d)",
                                      g_services[j].name,
                                      service_state_name(g_services[j].state),
                                      (int)g_services[j].pid);
                        }
                        break;
                    }

                    case SIGNAL_ACTION_NONE:
                    default:
                        break;
                }

            } else if (fd == ctl_fd) {
                /* Control socket client connected */
                ctl_server_accept(ctl_fd);
            } else if (fd == inotify_fd) {
                /* Service definitions changed */
                char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
                ssize_t len;
                while ((len = read(inotify_fd, buf, sizeof(buf))) > 0) {}
                LUNA_INFO("luna-init", "Service directory changed, reloading definitions");
                service_load_all(SERVICES_DIR);
                depgraph_build();
            } else if (fd == timer_fd) {
                uint64_t expirations;
                if (read(timer_fd, &expirations, sizeof(expirations)) > 0) {
                    supervisor_pump();
                }
            }
        }

        /* Check for boot completion asynchronously */
        if (!boot_complete && supervisor_is_boot_complete()) {
            boot_complete = true;
            LUNA_INFO("luna-init", "Stage 0 (v0.1) boot complete. Stages 5-7 pending.");
            splash_stop();
            luna_log_switch_to_runtime();
            console_print_welcome();
            
            pid_t shell_pid = fork();
            if (shell_pid == 0) {
                console_drop_to_shell();
                _exit(0);
            }
        }
    }

    /* ═══ Shutdown path ══════════════════════════════════════════════════ */

    LUNA_INFO("luna-init", "Event loop exited — shutting down");
    if (timer_fd >= 0) close(timer_fd);
    if (inotify_fd >= 0) close(inotify_fd);
    ctl_server_close(ctl_fd);
    signal_close(sigfd);
    close(epfd);
    luna_log_close();

    shutdown_run(SHUTDOWN_POWEROFF);
    /* shutdown_run() does not return */
    return 0;
}
