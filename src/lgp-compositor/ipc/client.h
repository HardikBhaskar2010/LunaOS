/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_IPC_CLIENT_H
#define MAHINA_LGP_IPC_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define LGP_CLIENT_RX_BUFFER_SIZE 4096
#define LGP_CLIENT_MAX_PENDING_FDS 8

typedef struct lgp_client {
    int fd;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    uint32_t session_id;
    uint32_t caps_granted;
    bool hello_done;
    size_t rx_len;
    uint8_t rx_buf[LGP_CLIENT_RX_BUFFER_SIZE];
    size_t pending_fd_count;
    int pending_fds[LGP_CLIENT_MAX_PENDING_FDS];
    struct lgp_client *next;
} lgp_client_t;

/*
 * lgp_client_create() — Allocate a new client object and read SO_PEERCRED.
 * Returns the client on success, NULL on failure.
 */
lgp_client_t *lgp_client_create(int fd);

/*
 * lgp_client_destroy() — Close the client fd and free memory.
 */
void lgp_client_destroy(lgp_client_t *client);

#endif
