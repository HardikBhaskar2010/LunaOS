/*
 * Copyright (c) 2026 Hardik Bhaskar
 * Licensed under the MIT License.
 *
 * luna-shell/wallpaper.h — Animated Luna Island video wallpaper engine.
 */

#ifndef LUNA_SHELL_WALLPAPER_H
#define LUNA_SHELL_WALLPAPER_H

#include <stdint.h>

/* Time-of-day modes (fallback) */
typedef enum {
    WALLPAPER_MORNING   = 0,
    WALLPAPER_AFTERNOON = 1,
    WALLPAPER_NIGHT     = 2
} wallpaper_tod_t;

/* Wallpaper engine state */
typedef struct {
    void *engine; /* Owned by luna-wallpaper-rs. */
} wallpaper_state_t;

void wallpaper_init(wallpaper_state_t *ws, uint32_t width, uint32_t height);
void wallpaper_tick(wallpaper_state_t *ws);
void wallpaper_render(const wallpaper_state_t *ws, void *pixels, uint32_t stride);
void wallpaper_cleanup(wallpaper_state_t *ws);

/* Determine time-of-day from uptime seconds */
wallpaper_tod_t wallpaper_get_tod(void);

#endif /* LUNA_SHELL_WALLPAPER_H */
