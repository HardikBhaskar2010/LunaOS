/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "config/args.h"
#include "logging/log.h"
#include "util/signal.h"
#include "drm/device.h"
#include "drm/connector.h"
#include "kms/crtc.h"
#include "kms/page_flip.h"
#include "kms/dumb_buffer.h"
#include "ipc/socket_server.h"
#include "ipc/client.h"
#include "protocol/tlv.h"
#include "protocol/hello.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 16
#define RUNTIME_LOG_PATH "/var/log/luna-init/runtime.log"

typedef struct {
    int epoll_fd;
    int signal_fd;
    bool running;
} lgp_compositor_state_t;

int main(int argc, char **argv) {
    lgp_args_t args;
    if (lgp_args_parse(argc, argv, &args) != 0) {
        return 1;
    }

    if (args.help) {
        printf("Usage: lgp-compositor [OPTIONS]\n");
        printf("Options:\n");
        printf("  -h, --help     Show this help message\n");
        printf("  -v, --version  Show version information\n");
        return 0;
    }

    if (args.version) {
        printf("lgp-compositor v0.1\n");
        return 0;
    }

    /* Initialize logging */
    if (lgp_log_init(RUNTIME_LOG_PATH) != 0) {
        fprintf(stderr, "lgp-compositor: failed to initialize log\n");
        return 1;
    }

    LGP_INFO("main", "Starting lgp-compositor (Mahina OS)");

    lgp_compositor_state_t state = {
        .epoll_fd = epoll_create1(EPOLL_CLOEXEC),
        .signal_fd = lgp_signal_init(),
        .running = true
    };

    if (state.epoll_fd < 0 || state.signal_fd < 0) {
        LGP_FATAL("main", "Failed to initialize epoll/signalfd");
        lgp_log_close();
        return 1;
    }

    /* Register signalfd with epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = state.signal_fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, state.signal_fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add signalfd to epoll");
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* M1: Raw DRM/KMS Modesetting */
    drm_device_t drm_dev;
    if (drm_device_open(&drm_dev, "/dev/dri/card0") != 0) {
        LGP_FATAL("main", "DRM device lost (could not open)");
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    if (drm_connector_setup(&drm_dev) != 0) {
        LGP_FATAL("main", "Failed to find suitable DRM connector/mode");
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    drm_dev.front_buffer = kms_dumb_buffer_create(&drm_dev, drm_dev.mode.hdisplay, drm_dev.mode.vdisplay);
    if (!drm_dev.front_buffer) {
        LGP_FATAL("main", "Failed to create front buffer");
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* Paint the first frame solid black (LUNA Void) per DL-043 */
    memset(drm_dev.front_buffer->map, 0, drm_dev.front_buffer->size);

    if (kms_crtc_commit(&drm_dev, drm_dev.front_buffer) != 0) {
        LGP_FATAL("main", "Failed to commit CRTC mode");
        kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    /* Add DRM FD to epoll to handle page flip events */
    ev.events = EPOLLIN;
    ev.data.fd = drm_dev.fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, drm_dev.fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add DRM fd to epoll");
    }

    /* M2: Socket server */
    int listen_fd = lgp_socket_server_init();
    if (listen_fd < 0) {
        LGP_FATAL("main", "Failed to initialize socket server");
        kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
        drm_device_close(&drm_dev);
        close(state.signal_fd);
        close(state.epoll_fd);
        lgp_log_close();
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        LGP_FATAL("main", "Failed to add listen fd to epoll");
    }

    /* Main Event Loop */
    struct epoll_event events[MAX_EVENTS];
    while (state.running) {
        int n = epoll_wait(state.epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            continue; /* EINTR handled implicitly */
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == state.signal_fd) {
                lgp_signal_action_t action = lgp_signal_read(state.signal_fd);
                if (action == LGP_SIGNAL_SHUTDOWN) {
                    LGP_INFO("main", "Initiating shutdown");
                    state.running = false;
                }
            } else if (fd == drm_dev.fd) {
                kms_handle_events(&drm_dev);
            } else if (fd == listen_fd) {
                int client_fd = lgp_socket_server_accept(listen_fd);
                if (client_fd >= 0) {
                    /* For M3, just add the fd to epoll. A robust client list is M4+ */
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(state.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        close(client_fd);
                    }
                }
            } else {
                /* Must be a client fd (in M3 we only have 1 global client tracking for simplicity) */
                uint8_t buf[1024];
                ssize_t s = read(fd, buf, sizeof(buf));
                if (s <= 0) {
                    close(fd);
                    epoll_ctl(state.epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                } else {
                    lgp_msg_t msg;
                    if (lgp_tlv_decode(buf, s, &msg)) {
                        if (msg.type == LGP_MSG_HELLO) {
                            lgp_client_t *client = lgp_client_create(fd);
                            if (client) {
                                lgp_hello_handle(client, &msg);
                                /* Destroy the client object, but we keep the fd alive for this mock M3 */
                                lgp_client_destroy(client);
                            }
                        } else if (msg.type == LGP_MSG_FILL_RECT) {
                            if (msg.length - LGP_HEADER_SIZE >= 4) {
                                uint8_t r = msg.payload[0];
                                uint8_t g = msg.payload[1];
                                uint8_t b = msg.payload[2];
                                /* uint8_t a = msg.payload[3]; */ /* Ignored for solid screen fill */
                                
                                uint32_t color = (r << 16) | (g << 8) | b;
                                LGP_INFO("main", "Client requested FILL_RECT with color #%06X", color);
                                
                                uint32_t *pixels = (uint32_t *)drm_dev.front_buffer->map;
                                size_t pixel_count = drm_dev.front_buffer->size / 4;
                                for (size_t p = 0; p < pixel_count; p++) {
                                    pixels[p] = color;
                                }
                                
                                /* For M3 mock: schedule page flip again to show the color */
                                kms_page_flip(&drm_dev, drm_dev.front_buffer, NULL);
                            }
                        }
                    }
                }
            }
        }
    }

    /* Clean up */
    LGP_INFO("main", "lgp-compositor shutting down cleanly");
    
    close(listen_fd);
    unlink(LGP_SOCKET_PATH);
    
    kms_dumb_buffer_destroy(&drm_dev, drm_dev.front_buffer);
    drm_device_close(&drm_dev);
    close(state.signal_fd);
    close(state.epoll_fd);
    lgp_log_close();

    return 0;
}
