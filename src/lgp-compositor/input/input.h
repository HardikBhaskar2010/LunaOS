/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_COMPOSITOR_INPUT_H
#define MAHINA_LGP_COMPOSITOR_INPUT_H

#include "../main.h"

#include <stdint.h>

/*
 * LGP input backend.
 *
 * Mahina keeps its own compositor and display protocol. This module only
 * normalizes kernel input events into LGP pointer/key messages. When libinput
 * is available at build time, it owns device discovery and event decoding.
 * Otherwise the old raw evdev/PS2 prototype path remains as a bring-up
 * fallback.
 */
void lgp_input_init(uint32_t screen_width, uint32_t screen_height);
int  lgp_input_get_fd(void);
int  lgp_input_get_aux_fd(void);
void lgp_input_pump(lgp_compositor_state_t *state);
void lgp_input_cleanup(void);

#endif
