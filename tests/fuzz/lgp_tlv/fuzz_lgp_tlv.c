/*
 * fuzz_lgp_tlv.c
 */

#include "../../../src/lgp-compositor/protocol/surface.h"
#include "../../../src/lgp-compositor/protocol/tlv.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t type = 0;
    uint32_t length = 0;
    lgp_msg_t msg;

    if (lgp_tlv_peek_header(data, size, &type, &length) &&
        lgp_tlv_decode(data, size, &msg)) {
        lgp_surface_create_payload_t create_payload;
        lgp_surface_destroy_payload_t destroy_payload;
        lgp_surface_commit_payload_t commit_payload;

        (void)type;
        (void)length;
        (void)lgp_surface_decode_create(&msg, &create_payload);
        (void)lgp_surface_decode_destroy(&msg, &destroy_payload);
        (void)lgp_surface_decode_commit(&msg, &commit_payload);
    }

    return 0;
}
