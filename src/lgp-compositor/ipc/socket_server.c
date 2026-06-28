/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "socket_server.h"
#include "../logging/log.h"
#include <errno.h>
#include <grp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

int lgp_socket_server_init(void) {
    /* Ensure the directory exists */
    mkdir("/run/lgp", 0755);

    /* Clean up stale socket from previous crash */
    struct stat st = {0};
    if (stat(LGP_SOCKET_PATH, &st) == 0) {
        if (unlink(LGP_SOCKET_PATH) != 0) {
            LGP_ERROR("ipc", "Failed to unlink stale socket: %s", strerror(errno));
            return -1;
        }
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LGP_ERROR("ipc", "Failed to create unix socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LGP_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        LGP_ERROR("ipc", "Failed to bind to %s: %s", LGP_SOCKET_PATH, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 32) != 0) {
        LGP_ERROR("ipc", "Failed to listen on socket: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* Set permissions to 0660 (rw-rw----) and ownership to root:video */
    chmod(LGP_SOCKET_PATH, 0660);
    struct group *gr = getgrnam("video");
    if (gr) {
        chown(LGP_SOCKET_PATH, 0, gr->gr_gid);
    } else {
        LGP_WARN("ipc", "Group 'video' not found, socket may not be accessible to clients");
    }

    LGP_INFO("ipc", "Listening for LGP connections on %s", LGP_SOCKET_PATH);
    return fd;
}

int lgp_socket_server_accept(int listen_fd) {
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept4(listen_fd, (struct sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            LGP_ERROR("ipc", "accept4 failed: %s", strerror(errno));
        }
        return -1;
    }

    LGP_DEBUG("ipc", "Accepted new LGP connection (fd %d)", client_fd);
    return client_fd;
}
