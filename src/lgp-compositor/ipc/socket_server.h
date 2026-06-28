/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_IPC_SOCKET_SERVER_H
#define MAHINA_LGP_IPC_SOCKET_SERVER_H

#define LGP_SOCKET_PATH "/run/lgp/compositor.sock"

/*
 * lgp_socket_server_init() — Bind and listen on the LGP socket.
 * Cleans up stale sockets if they exist.
 * Returns the bound socket fd on success, -1 on failure.
 */
int lgp_socket_server_init(void);

/*
 * lgp_socket_server_accept() — Accept a new connection from the listening socket.
 * Returns the client fd on success, -1 on failure.
 */
int lgp_socket_server_accept(int listen_fd);

#endif
