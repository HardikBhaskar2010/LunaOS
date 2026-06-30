/*
 * Copyright (c) 2026 Hardik Bhaskar
 * Licensed under the MIT License.
 *
 * tools/gen_video.c — Synthwave loop video generator.
 *
 * Generates a 30-frame looping synthwave animation in uncompressed ARGB8888.
 * Output format (.lraw):
 *   char magic[4] = "LRAW"
 *   uint32_t width = 480
 *   uint32_t height = 270
 *   uint32_t frame_count = 30
 *   Followed by width * height * 4 * frame_count bytes of raw pixels.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WIDTH        480
#define HEIGHT       270
#define FRAMES       30
#define HORIZON      140

/* Color helpers */
static uint32_t blend(uint32_t c1, uint32_t c2, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    uint32_t r1 = (c1 >> 16) & 0xFF;
    uint32_t g1 = (c1 >> 8) & 0xFF;
    uint32_t b1 = c1 & 0xFF;
    uint32_t r2 = (c2 >> 16) & 0xFF;
    uint32_t g2 = (c2 >> 8) & 0xFF;
    uint32_t b2 = c2 & 0xFF;
    uint32_t r = (uint32_t)(r1 * (1.0f - t) + r2 * t);
    uint32_t g = (uint32_t)(g1 * (1.0f - t) + g2 * t);
    uint32_t b = (uint32_t)(b1 * (1.0f - t) + b2 * t);
    return (0xFFU << 24) | (r << 16) | (g << 8) | b;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
        return 1;
    }

    FILE *out = fopen(argv[1], "wb");
    if (!out) {
        perror("Failed to open output file");
        return 1;
    }

    /* Write LRAW header */
    fwrite("LRAW", 1, 4, out);
    uint32_t w = WIDTH;
    uint32_t h = HEIGHT;
    uint32_t fc = FRAMES;
    fwrite(&w, 4, 1, out);
    fwrite(&h, 4, 1, out);
    fwrite(&fc, 4, 1, out);

    uint32_t *frame = malloc(WIDTH * HEIGHT * sizeof(uint32_t));
    if (!frame) {
        fclose(out);
        return 1;
    }

    /* Animation generation loop */
    for (int f = 0; f < FRAMES; f++) {
        float time_t = (float)f / FRAMES;

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                uint32_t color = 0xFF05050A; /* Default space black */

                if (y < HORIZON) {
                    /* 1. Sky Gradient: Deep Purple/Black to Crimson Horizon */
                    float sky_t = (float)y / HORIZON;
                    color = blend(0xFF070014, 0xFF3C0E40, sky_t);

                    /* 2. Cyberpunk Giant Sun (Center at 240, 95, radius 40) */
                    float dx = x - 240.0f;
                    float dy = y - 95.0f;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 42.0f) {
                        /* Sun color gradient: Yellow to Neon Pink */
                        float sun_t = (float)(y - 53) / 84.0f;
                        uint32_t sun_col = blend(0xFFFFE000, 0xFFFF007F, sun_t);

                        /* Classic horizontal synthwave slices */
                        int slice_y = y - 53;
                        bool in_slice = false;
                        /* Slices get progressively wider as they go down */
                        int band_width = 3 + (slice_y / 10);
                        int spacing = 12;
                        if ((slice_y % spacing) < band_width && slice_y > 10) {
                            in_slice = true;
                        }

                        if (!in_slice) {
                            color = sun_col;
                        }
                    }

                    /* 3. Mountain outlines */
                    /* Left mountain */
                    float l_mtn = 0.0f;
                    if (x < 180) {
                        l_mtn = (180 - x) * 0.35f;
                        /* add noise/jaggedness */
                        l_mtn += sinf(x * 0.15f) * 2.0f + cosf(x * 0.3f) * 1.0f;
                    }
                    /* Right mountain */
                    float r_mtn = 0.0f;
                    if (x > 300) {
                        r_mtn = (x - 300) * 0.35f;
                        r_mtn += sinf(x * 0.12f) * 2.0f + cosf(x * 0.25f) * 1.0f;
                    }
                    float mtn_h = l_mtn > r_mtn ? l_mtn : r_mtn;
                    if (mtn_h > 0.0f && y >= HORIZON - (int)mtn_h && y < HORIZON) {
                        /* Draw silhouette or soft edge glow */
                        if (y < HORIZON - (int)mtn_h + 2) {
                            color = 0xFF8A2BE2; /* Purple peak glow */
                        } else {
                            color = 0xFF0D081F; /* Dark mountain body */
                        }
                    }

                } else {
                    /* 4. Floor perspective grid */
                    float floor_t = (float)(y - HORIZON) / (HEIGHT - HORIZON);
                    
                    /* Floor gradient: Deep Blue/Violet transitioning to brighter glow */
                    color = blend(0xFF140C24, 0xFF080210, floor_t);

                    /* Perspective grid vertical lines */
                    /* Radiate from horizon center (240, HORIZON) */
                    float grid_dx = x - 240.0f;
                    float angle = grid_dx / (y - HORIZON + 8.0f);
                    float grid_spacing_v = 0.18f;
                    
                    /* Draw grid line if close to vertical spacing bounds */
                    float val = angle / grid_spacing_v;
                    float diff = fabsf(val - roundf(val)) * (y - HORIZON + 4.0f);
                    bool is_grid_v = (diff < 0.95f);

                    /* Horizontal lines: move towards screen using time_t */
                    /* Line spacing increases exponentially */
                    bool is_grid_h = false;
                    for (int i = 0; i < 15; i++) {
                        float line_z = (float)i - time_t;
                        /* Exponential perspective mapping */
                        float ly = HORIZON + (HEIGHT - HORIZON) * (powf(1.42f, line_z) / powf(1.42f, 14.0f));
                        if (fabsf((float)y - ly) < 0.65f + (floor_t * 0.75f)) {
                            is_grid_h = true;
                            break;
                        }
                    }

                    if (is_grid_v || is_grid_h) {
                        /* Neon magenta or cyan grid with perspective intensity fade */
                        uint32_t grid_col = blend(0xFFE03E8A, 0xFF00FFFF, time_t);
                        color = blend(color, grid_col, 0.2f + 0.8f * floor_t);
                    }
                }

                frame[y * WIDTH + x] = color;
            }
        }

        fwrite(frame, sizeof(uint32_t), WIDTH * HEIGHT, out);
    }

    free(frame);
    fclose(out);
    return 0;
}
