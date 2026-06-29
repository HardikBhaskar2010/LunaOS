/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "client.h"
#include "../logging/log.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

lgp_client_t *lgp_client_create(int fd) {
    lgp_client_t *client = calloc(1, sizeof(*client));
    if (!client) {
        LGP_ERROR("ipc", "Failed to allocate client struct");
        return NULL;
    }

    client->fd = fd;
    client->pid = -1;
    client->uid = (uid_t)-1;
    client->gid = (gid_t)-1;
    for (size_t i = 0; i < LGP_CLIENT_MAX_PENDING_FDS; i++) {
        client->pending_fds[i] = -1;
    }

    struct ucred ucred;
    socklen_t len = sizeof(struct ucred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
        LGP_ERROR("ipc", "getsockopt(SO_PEERCRED) failed: %s", strerror(errno));
        /* Continue anyway, but PID/UID will be 0 */
    } else {
        client->pid = ucred.pid;
        client->uid = ucred.uid;
        client->gid = ucred.gid;
        LGP_DEBUG("ipc", "Client SO_PEERCRED: pid=%d uid=%d gid=%d", 
                  (int)client->pid, (int)client->uid, (int)client->gid);
    }

    return client;
}

void lgp_client_destroy(lgp_client_t *client) {
    if (!client) return;
    
    LGP_DEBUG("ipc", "Destroying client (pid %d)", (int)client->pid);
    if (client->fd >= 0) {
        close(client->fd);
    }
    for (size_t i = 0; i < client->pending_fd_count; i++) {
        if (client->pending_fds[i] >= 0) {
            close(client->pending_fds[i]);
        }
    }
    free(client);
}
