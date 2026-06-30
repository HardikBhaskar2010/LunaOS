/*
 * protocol_test.c
 */

#include "unity.h"

#include "../../../src/lgp-compositor/ipc/client.h"
#include "../../../src/lgp-compositor/protocol/caps.h"
#include "../../../src/lgp-compositor/protocol/surface.h"
#include "../../../src/lgp-compositor/protocol/tlv.h"
#include "../../../src/lgp-compositor/scene/surface.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

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

static int create_test_shm(size_t size, uint32_t fill, void **out_map) {
    char name[64];
    snprintf(name, sizeof(name), "/mahina-lgp-unit-%ld", (long)getpid());

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
    shm_unlink(name);
    TEST_ASSERT_EQUAL_INT(0, ftruncate(fd, (off_t)size));

    void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    TEST_ASSERT_NOT_EQUAL(MAP_FAILED, map);

    uint32_t *pixels = (uint32_t *)map;
    for (size_t i = 0; i < size / sizeof(uint32_t); i++) {
        pixels[i] = fill;
    }
    *out_map = map;
    return fd;
}

void test_tlv_rejects_partial_frame(void);
void test_tlv_rejects_partial_frame(void) {
    uint8_t frame[LGP_HEADER_SIZE + 4] = {0};
    write_u16_le(frame, LGP_MSG_CREATE_SURFACE);
    write_u32_le(frame + 2, sizeof(frame));

    uint16_t type = 0;
    uint32_t length = 0;
    TEST_ASSERT_TRUE(lgp_tlv_peek_header(frame, LGP_HEADER_SIZE, &type, &length));
    TEST_ASSERT_EQUAL_UINT16(LGP_MSG_CREATE_SURFACE, type);
    TEST_ASSERT_EQUAL_UINT32(sizeof(frame), length);

    lgp_msg_t msg;
    TEST_ASSERT_FALSE(lgp_tlv_decode(frame, LGP_HEADER_SIZE, &msg));
    TEST_ASSERT_TRUE(lgp_tlv_decode(frame, sizeof(frame), &msg));
    TEST_ASSERT_EQUAL_UINT16(LGP_MSG_CREATE_SURFACE, msg.type);
}

void test_surface_decode_rejects_bad_payload_length(void);
void test_surface_decode_rejects_bad_payload_length(void) {
    uint8_t payload[20] = {0};
    lgp_msg_t msg = {
        .type = LGP_MSG_CREATE_SURFACE,
        .length = LGP_HEADER_SIZE + sizeof(payload),
        .payload = payload
    };
    lgp_surface_create_payload_t decoded;
    TEST_ASSERT_FALSE(lgp_surface_decode_create(&msg, &decoded));
}

void test_surface_lifecycle_and_client_cleanup(void);
void test_surface_lifecycle_and_client_cleanup(void) {
    lgp_surface_manager_t manager;
    lgp_surface_manager_init(&manager);

    lgp_client_t client = {
        .session_id = 7,
        .caps_granted = LGP_CAP_CANVAS_SURFACE | LGP_CAP_DIRECT_LGP
    };
    lgp_surface_create_payload_t create = {
        .surface_type = LGP_SURFACE_CANVAS_SURFACE,
        .x = 2,
        .y = 3,
        .width = 16,
        .height = 12,
        .layer = LGP_LAYER_APPLICATION
    };

    uint32_t surface_id = 0;
    TEST_ASSERT_EQUAL_UINT32(LGP_SURFACE_STATUS_OK,
                             lgp_surface_manager_create(&manager, &client, &create,
                                                        128, 128, &surface_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, surface_id);
    TEST_ASSERT_NOT_NULL(lgp_surface_manager_find(&manager, surface_id));

    lgp_surface_manager_destroy_for_client(&manager, &client);
    TEST_ASSERT_NULL(lgp_surface_manager_find(&manager, surface_id));
}

void test_surface_denies_privileged_surface_without_cap(void);
void test_surface_denies_privileged_surface_without_cap(void) {
    lgp_surface_manager_t manager;
    lgp_surface_manager_init(&manager);

    lgp_client_t client = {
        .session_id = 8,
        .caps_granted = 0
    };
    lgp_surface_create_payload_t create = {
        .surface_type = LGP_SURFACE_LUNA_ISLAND,
        .x = 0,
        .y = 0,
        .width = 32,
        .height = 32,
        .layer = LGP_LAYER_LUNA_ISLAND
    };

    uint32_t surface_id = 0;
    TEST_ASSERT_EQUAL_UINT32(LGP_SURFACE_STATUS_DENIED,
                             lgp_surface_manager_create(&manager, &client, &create,
                                                        128, 128, &surface_id));
    TEST_ASSERT_EQUAL_UINT32(0, surface_id);
}

void test_commit_shm_and_composite(void);
void test_commit_shm_and_composite(void) {
    lgp_surface_manager_t manager;
    lgp_surface_manager_init(&manager);

    lgp_client_t client = {
        .session_id = 9,
        .caps_granted = LGP_CAP_CANVAS_SURFACE | LGP_CAP_DIRECT_LGP
    };
    lgp_surface_create_payload_t create = {
        .surface_type = LGP_SURFACE_CANVAS_SURFACE,
        .x = 4,
        .y = 5,
        .width = 4,
        .height = 4,
        .layer = LGP_LAYER_APPLICATION
    };

    uint32_t surface_id = 0;
    TEST_ASSERT_EQUAL_UINT32(LGP_SURFACE_STATUS_OK,
                             lgp_surface_manager_create(&manager, &client, &create,
                                                        32, 32, &surface_id));

    void *map = NULL;
    int fd = create_test_shm(4u * 4u * sizeof(uint32_t), 0x00112233u, &map);
    lgp_surface_commit_payload_t commit = {
        .surface_id = surface_id,
        .width = 4,
        .height = 4,
        .stride = 4u * sizeof(uint32_t),
        .pixel_format = LGP_PIXEL_FORMAT_XRGB8888,
        .byte_size = 4u * 4u * sizeof(uint32_t)
    };

    TEST_ASSERT_EQUAL_UINT32(LGP_SURFACE_STATUS_OK,
                             lgp_surface_manager_commit(&manager, &client, &commit, fd));

    uint32_t framebuffer[32u * 32u];
    memset(framebuffer, 0, sizeof(framebuffer));
    TEST_ASSERT_EQUAL_INT(0, lgp_surface_manager_composite(&manager,
                                                           framebuffer,
                                                           32,
                                                           32,
                                                           32u * sizeof(uint32_t)));
    TEST_ASSERT_EQUAL_HEX32(0x00112233u, framebuffer[(5u * 32u) + 4u]);

    munmap(map, 4u * 4u * sizeof(uint32_t));
    lgp_surface_manager_destroy_for_client(&manager, &client);
}

#include "../../../src/lgp-compositor/protocol/wm.h"

void test_wm_surface_created_encoding(void);
void test_wm_surface_created_encoding(void) {
    uint8_t buf[LGP_HEADER_SIZE + 16];
    TEST_ASSERT_TRUE(lgp_wm_encode_surface_created(buf, sizeof(buf), 42, 4, 100, 200));

    uint16_t type = 0;
    uint32_t len = 0;
    TEST_ASSERT_TRUE(lgp_tlv_peek_header(buf, sizeof(buf), &type, &len));
    TEST_ASSERT_EQUAL_UINT16(0x0200u, type); /* LGP_MSG_WM_SURFACE_CREATED */
    TEST_ASSERT_EQUAL_UINT32(sizeof(buf), len);
}

void test_wm_decode_set_position(void);
void test_wm_decode_set_position(void) {
    uint8_t payload[12];
    write_u32_le(payload + 0, 42); // surface_id
    write_u32_le(payload + 4, 150); // x
    write_u32_le(payload + 8, 250); // y

    lgp_msg_t msg = {
        .type = LGP_MSG_WM_SET_SURFACE_POSITION,
        .length = LGP_HEADER_SIZE + sizeof(payload),
        .payload = payload
    };

    lgp_wm_set_position_payload_t decoded;
    TEST_ASSERT_TRUE(lgp_wm_decode_set_position(&msg, &decoded));
    TEST_ASSERT_EQUAL_UINT32(42, decoded.surface_id);
    TEST_ASSERT_EQUAL_INT32(150, decoded.x);
    TEST_ASSERT_EQUAL_INT32(250, decoded.y);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tlv_rejects_partial_frame);
    RUN_TEST(test_surface_decode_rejects_bad_payload_length);
    RUN_TEST(test_surface_lifecycle_and_client_cleanup);
    RUN_TEST(test_surface_denies_privileged_surface_without_cap);
    RUN_TEST(test_commit_shm_and_composite);
    RUN_TEST(test_wm_surface_created_encoding);
    RUN_TEST(test_wm_decode_set_position);
    return UNITY_END();
}
