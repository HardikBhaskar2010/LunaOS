/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_LGP_COMPOSITOR_INPUT_KEYBOARD_H
#define MAHINA_LGP_COMPOSITOR_INPUT_KEYBOARD_H

#include "../main.h"

/*
 * lgp_keyboard_init() — Initialize keyboard evdev reading.
 */
void lgp_keyboard_init(void);

/*
 * lgp_keyboard_pump() — Read pending evdev events and dispatch to the compositor state.
 */
void lgp_keyboard_pump(lgp_compositor_state_t *state);

/*
 * lgp_keyboard_get_fd() — Return the evdev fd for the event loop, or -1.
 */
int lgp_keyboard_get_fd(void);

#endif
