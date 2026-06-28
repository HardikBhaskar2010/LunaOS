/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_ARGS_H
#define MAHINA_LGP_ARGS_H

#include <stdbool.h>

typedef struct {
    bool help;
    bool version;
} lgp_args_t;

/*
 * lgp_args_parse() — Parse command line arguments.
 * Returns 0 on success, -1 on failure.
 */
int lgp_args_parse(int argc, char **argv, lgp_args_t *out_args);

#endif
