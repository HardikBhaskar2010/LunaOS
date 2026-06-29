/*
 * fuzz_toml.c — AFL++ fuzzing harness for Mahina TOML parser
 * Authority: Volume VI / 01_coding_standards.md §5 (Fuzzing Requirements)
 *
 * This file is meant to be compiled with afl-clang-lto.
 * It feeds arbitrary fuzz data directly into the TOML parser to ensure
 * no buffer overflows, infinite loops, or crashes occur.
 */

#include "../../../src/luna-init/toml.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void fuzz_one_input(const uint8_t *buf, size_t len) {
    if (!buf || len < 3 || len > TOML_MAX_DOC_BYTES) return;

    char *str = malloc(len + 1);
    if (!str) return;
    memcpy(str, buf, len);
    str[len] = '\0';

    toml_error_t err;
    toml_doc_t *doc = toml_load_buffer(str, len, &err);
    if (doc) toml_free(doc);
    free(str);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_one_input(data, size);
    return 0;
}

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();

int main(void) {
    __AFL_INIT();

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        fuzz_one_input(buf, (size_t)__AFL_FUZZ_TESTCASE_LEN);
    }

    return 0;
}
#endif
