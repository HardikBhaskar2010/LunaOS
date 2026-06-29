/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "protocol/caps.h"
#include "protocol/surface.h"
#include "protocol/tlv.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define TEST_SURFACE_WIDTH 360u
#define TEST_SURFACE_HEIGHT 220u
#define TEST_SURFACE_STRIDE (TEST_SURFACE_WIDTH * 4u)
#define TEST_SURFACE_SIZE (TEST_SURFACE_STRIDE * TEST_SURFACE_HEIGHT)

static void write_u16_le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
    p[2] = (uint8_t)((value >> 16) & 0xFFu);
    p[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void write_i32_le(uint8_t *p, int32_t value) {
    write_u32_le(p, (uint32_t)value);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int write_all(int fd, const uint8_t *buf, size_t len) {
    size_t written = 0;

    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        written += (size_t)n;
    }

    return 0;
}

static int read_exact(int fd, uint8_t *buf, size_t len) {
    size_t read_len = 0;

    while (read_len < len) {
        ssize_t n = read(fd, buf + read_len, len - read_len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        read_len += (size_t)n;
    }

    return 0;
}

static int send_with_fd(int socket_fd, const uint8_t *buf, size_t len, int payload_fd) {
    struct iovec iov = {
        .iov_base = (void *)buf,
        .iov_len = len
    };
    uint8_t control[CMSG_SPACE(sizeof(int))] = {0};
    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control,
        .msg_controllen = sizeof(control)
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &payload_fd, sizeof(payload_fd));

    ssize_t n = sendmsg(socket_fd, &msg, 0);
    if (n < 0 || (size_t)n != len) {
        return -1;
    }
    return 0;
}

static void encode_header(uint8_t *buf, uint16_t type, uint32_t length) {
    write_u16_le(buf, type);
    write_u32_le(buf + 2, length);
}

static void render_pattern(uint32_t *pixels) {
    for (uint32_t y = 0; y < TEST_SURFACE_HEIGHT; y++) {
        for (uint32_t x = 0; x < TEST_SURFACE_WIDTH; x++) {
            uint8_t r = (uint8_t)((x * 255u) / TEST_SURFACE_WIDTH);
            uint8_t g = (uint8_t)((y * 255u) / TEST_SURFACE_HEIGHT);
            uint8_t b = 0x80u;

            if (x < 8 || y < 8 || x >= TEST_SURFACE_WIDTH - 8 || y >= TEST_SURFACE_HEIGHT - 8) {
                r = 0x48u;
                g = 0xD1u;
                b = 0xCCu;
            }

            pixels[(size_t)y * TEST_SURFACE_WIDTH + x] =
                ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}

static int create_shm_buffer(void **out_map) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/mahina-lgp-test-%ld", (long)getpid());

    int fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        return -1;
    }
    shm_unlink(shm_name);

    if (ftruncate(fd, TEST_SURFACE_SIZE) != 0) {
        close(fd);
        return -1;
    }

    void *map = mmap(NULL, TEST_SURFACE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return -1;
    }

    *out_map = map;
    return fd;
}

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

    printf("Connected to LGP compositor.\n");

    uint8_t hello[LGP_HEADER_SIZE + 8] = {0};
    encode_header(hello, LGP_MSG_HELLO, sizeof(hello));
    write_u16_le(hello + 6, 1);
    write_u16_le(hello + 8, 0);
    write_u32_le(hello + 10, LGP_CAP_CANVAS_SURFACE | LGP_CAP_DIRECT_LGP);

    if (write_all(fd, hello, sizeof(hello)) != 0) {
        perror("write hello");
        close(fd);
        return 1;
    }

    uint8_t hello_reply[LGP_HEADER_SIZE + 8];
    if (read_exact(fd, hello_reply, sizeof(hello_reply)) != 0) {
        perror("read hello reply");
        close(fd);
        return 1;
    }
    uint32_t caps_granted = read_u32_le(hello_reply + 10);
    printf("HELLO complete. Caps granted: 0x%08x\n", caps_granted);

    uint8_t create[LGP_HEADER_SIZE + 24] = {0};
    encode_header(create, LGP_MSG_CREATE_SURFACE, sizeof(create));
    write_u32_le(create + 6, LGP_SURFACE_CANVAS_SURFACE);
    write_i32_le(create + 10, 96);
    write_i32_le(create + 14, 84);
    write_u32_le(create + 18, TEST_SURFACE_WIDTH);
    write_u32_le(create + 22, TEST_SURFACE_HEIGHT);
    write_u32_le(create + 26, LGP_LAYER_APPLICATION);

    if (write_all(fd, create, sizeof(create)) != 0) {
        perror("write create surface");
        close(fd);
        return 1;
    }

    uint8_t create_reply[LGP_HEADER_SIZE + 8];
    if (read_exact(fd, create_reply, sizeof(create_reply)) != 0) {
        perror("read create reply");
        close(fd);
        return 1;
    }

    uint32_t status = read_u32_le(create_reply + LGP_HEADER_SIZE);
    uint32_t surface_id = read_u32_le(create_reply + LGP_HEADER_SIZE + 4);
    if (status != LGP_SURFACE_STATUS_OK || surface_id == 0) {
        fprintf(stderr, "CREATE_SURFACE rejected: status=%u surface_id=%u\n", status, surface_id);
        close(fd);
        return 1;
    }
    printf("Created surface id=%u.\n", surface_id);

    void *map = NULL;
    int shm_fd = create_shm_buffer(&map);
    if (shm_fd < 0) {
        perror("shm_open/ftruncate/mmap");
        close(fd);
        return 1;
    }

    render_pattern((uint32_t *)map);

    uint8_t commit[LGP_HEADER_SIZE + 24] = {0};
    encode_header(commit, LGP_MSG_COMMIT_BUFFER, sizeof(commit));
    write_u32_le(commit + 6, surface_id);
    write_u32_le(commit + 10, TEST_SURFACE_WIDTH);
    write_u32_le(commit + 14, TEST_SURFACE_HEIGHT);
    write_u32_le(commit + 18, TEST_SURFACE_STRIDE);
    write_u32_le(commit + 22, LGP_PIXEL_FORMAT_XRGB8888);
    write_u32_le(commit + 26, TEST_SURFACE_SIZE);

    if (send_with_fd(fd, commit, sizeof(commit), shm_fd) != 0) {
        perror("send commit buffer");
        munmap(map, TEST_SURFACE_SIZE);
        close(shm_fd);
        close(fd);
        return 1;
    }

    printf("Committed shared-memory surface. Check the QEMU display.\n");
    sleep(10);

    uint8_t destroy[LGP_HEADER_SIZE + 4] = {0};
    encode_header(destroy, LGP_MSG_DESTROY_SURFACE, sizeof(destroy));
    write_u32_le(destroy + 6, surface_id);
    (void)write_all(fd, destroy, sizeof(destroy));

    munmap(map, TEST_SURFACE_SIZE);
    close(shm_fd);
    close(fd);
    return 0;
}
