/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_IPC_CLIENT_H
#define MAHINA_LGP_IPC_CLIENT_H

#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int fd;
    pid_t pid;
    uid_t uid;
    gid_t gid;
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
