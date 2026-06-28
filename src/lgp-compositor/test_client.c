/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/run/lgp/compositor.sock", sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("Connected to LGP Compositor!\n");

    /* Send LGP_HELLO */
    uint8_t hello[14] = {
        0x01, 0x00,             /* Type = 1 */
        0x0E, 0x00, 0x00, 0x00, /* Length = 14 */
        0x01, 0x00,             /* Major = 1 */
        0x00, 0x00,             /* Minor = 0 */
        0x00, 0x00, 0x00, 0x00  /* Caps = 0 */
    };
    
    if (write(fd, hello, sizeof(hello)) != sizeof(hello)) {
        perror("write hello");
        return 1;
    }
    printf("Sent LGP_HELLO.\n");

    /* Read LGP_HELLO_REPLY */
    uint8_t reply[14];
    if (read(fd, reply, sizeof(reply)) != sizeof(reply)) {
        perror("read reply");
        return 1;
    }
    printf("Received LGP_HELLO_REPLY!\n");

    /* Send LGP_MSG_FILL_RECT (Type 0x10) - BLUE */
    uint8_t fill[10] = {
        0x10, 0x00,             /* Type = 0x10 */
        0x0A, 0x00, 0x00, 0x00, /* Length = 10 */
        0x00, 0x00, 0xFF, 0xFF  /* R=0, G=0, B=255, A=255 */
    };

    if (write(fd, fill, sizeof(fill)) != sizeof(fill)) {
        perror("write fill");
        return 1;
    }
    printf("Sent FILL_RECT (Blue). Check your screen!\n");

    /* Keep connection open so compositor doesn't tear us down immediately */
    sleep(10);
    close(fd);
    return 0;
}
