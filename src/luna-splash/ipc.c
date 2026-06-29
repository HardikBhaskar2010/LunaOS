/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ipc_fd = -1;
static char buffer[MAX_IPC_MSG];
static int buf_len = 0;

int ipc_init(int fd) {
    if (fd < 0) return -1;
    ipc_fd = fd;
    buf_len = 0;
    return 0;
}

void ipc_cleanup(void) {
    if (ipc_fd >= 0) {
        close(ipc_fd);
        ipc_fd = -1;
    }
}

bool ipc_read_event(char *msg_out, int *percent_out) {
    if (ipc_fd < 0) return false;

    char *nl = strchr(buffer, '\n');
    if (!nl) {
        char temp[64];
        ssize_t n = read(ipc_fd, temp, sizeof(temp) - 1);
        if (n <= 0) return false;

        temp[n] = '\0';
        
        /* Append to buffer */
        int to_copy = (int)n;
        if (buf_len + to_copy >= MAX_IPC_MSG) {
            to_copy = MAX_IPC_MSG - buf_len - 1;
        }
        memcpy(buffer + buf_len, temp, (size_t)to_copy);
        buf_len += to_copy;
        buffer[buf_len] = '\0';

        nl = strchr(buffer, '\n');
    }

    if (nl) {
        *nl = '\0';
        
        /* Parse PERCENT|MSG */
        char *sep = strchr(buffer, '|');
        if (sep) {
            *sep = '\0';
            *percent_out = atoi(buffer);
            strncpy(msg_out, sep + 1, MAX_IPC_MSG - 1);
            msg_out[MAX_IPC_MSG - 1] = '\0';
        } else {
            *percent_out = 0;
            strncpy(msg_out, buffer, MAX_IPC_MSG - 1);
            msg_out[MAX_IPC_MSG - 1] = '\0';
        }

        /* Shift buffer */
        int remain = buf_len - (int)(nl - buffer) - 1;
        if (remain > 0) {
            memmove(buffer, nl + 1, (size_t)remain);
            buf_len = remain;
            buffer[buf_len] = '\0';
        } else {
            buf_len = 0;
            buffer[0] = '\0';
        }
        return true;
    }

    return false;
}
