/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "render.h"
#include "font8x16.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int fb_fd = -1;
static uint32_t *fb_mem = NULL;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

int screen_w = 0;
int screen_h = 0;
static int line_length = 0;

int render_init(void) {
    fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb_fd < 0) return -1;

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(fb_fd);
        return -1;
    }

    screen_w = (int)vinfo.xres;
    screen_h = (int)vinfo.yres;
    line_length = (int)finfo.line_length;

    size_t screensize = (size_t)vinfo.yres_virtual * (size_t)finfo.line_length;

    fb_mem = (uint32_t *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
    if (fb_mem == MAP_FAILED) {
        close(fb_fd);
        return -1;
    }

    return 0;
}

void render_cleanup(void) {
    if (fb_mem != MAP_FAILED && fb_mem != NULL) {
        size_t screensize = (size_t)vinfo.yres_virtual * (size_t)finfo.line_length;
        munmap(fb_mem, screensize);
        fb_mem = NULL;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
}

static inline void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= screen_w || y < 0 || y >= screen_h) return;
    
    if (vinfo.bits_per_pixel == 32) {
        int offset = (y * line_length / 4) + x;
        fb_mem[offset] = color;
    } else if (vinfo.bits_per_pixel == 16) {
        int offset = (y * line_length / 2) + x;
        uint16_t *fb16 = (uint16_t *)fb_mem;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint16_t color16 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        fb16[offset] = color16;
    } else if (vinfo.bits_per_pixel == 24) {
        int offset = (y * line_length) + (x * 3);
        uint8_t *fb8 = (uint8_t *)fb_mem;
        fb8[offset] = color & 0xFF;
        fb8[offset+1] = (color >> 8) & 0xFF;
        fb8[offset+2] = (color >> 16) & 0xFF;
    }
}

void render_fade_out(void) {
    if (!fb_mem) return;
    size_t pixel_count = (size_t)vinfo.yres_virtual * (size_t)(finfo.line_length / (vinfo.bits_per_pixel / 8));
    if (vinfo.bits_per_pixel != 32) return; /* simple fade only for 32bpp */

    uint32_t *backbuffer = malloc(pixel_count * sizeof(uint32_t));
    if (!backbuffer) return;
    
    /* Copy framebuffer to backbuffer once (slow read from VRAM) */
    memcpy(backbuffer, fb_mem, pixel_count * sizeof(uint32_t));

    for (int step = 0; step < 16; step++) {
        for (size_t i = 0; i < pixel_count; i++) {
            uint32_t c = backbuffer[i];
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = c & 0xFF;
            r = (r > 16) ? r - 16 : 0;
            g = (g > 16) ? g - 16 : 0;
            b = (b > 16) ? b - 16 : 0;
            backbuffer[i] = (r << 16) | (g << 8) | b;
        }
        /* Bulk copy back to VRAM (fast write) */
        memcpy(fb_mem, backbuffer, pixel_count * sizeof(uint32_t));
        usleep(30000);
    }
    
    free(backbuffer);
}

void render_clear(uint32_t color) {
    for (int y = 0; y < screen_h; y++) {
        for (int x = 0; x < screen_w; x++) {
            put_pixel(x, y, color);
        }
    }
}

void render_text(int x, int y, const char *text, uint32_t color) {
    int cur_x = x;
    int cur_y = y;
    
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c == '\n') {
            cur_x = x;
            cur_y += 16;
        } else {
            for (int cy = 0; cy < 16; cy++) {
                uint8_t row = font8x16[c][cy];
                for (int cx = 0; cx < 8; cx++) {
                    if (row & (1 << (7 - cx))) {
                        put_pixel(cur_x + cx, cur_y + cy, color);
                    }
                }
            }
            cur_x += 8;
        }
        text++;
    }
}

void render_text_centered(int y, const char *text, uint32_t color) {
    int len = 0;
    while (text[len]) len++;
    int total_width = len * 8;
    int x = (screen_w - total_width) / 2;
    render_text(x, y, text, color);
}

void render_logo(void) {
    int center_y = screen_h / 2 - 60;
    
    const char *logo[] = {
        "  ███╗   ███╗ █████╗ ██╗  ██╗██╗███╗   ██╗ █████╗ ",
        "  ████╗ ████║██╔══██╗██║  ██║██║████╗  ██║██╔══██╗",
        "  ██╔████╔██║███████║███████║██║██╔██╗ ██║███████║",
        "  ██║╚██╔╝██║██╔══██║██╔══██║██║██║╚██╗██║██╔══██║",
        "  ██║ ╚═╝ ██║██║  ██║██║  ██║██║██║ ╚████║██║  ██║",
        "  ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝",
        NULL
    };
    
    for (int y_idx = 0; logo[y_idx] != NULL; y_idx++) {
        const char *p = logo[y_idx];
        int cur_x = (screen_w - 46 * 8) / 2;
        int cur_y = center_y + y_idx * 16;
        
        while (*p) {
            if (*p == ' ') {
                cur_x += 8;
                p++;
            } else if ((unsigned char)*p == 0xE2) {
                /* Basic UTF-8 parsing for the specific block chars used in the logo.
                 * █ = E2 96 88
                 * ╗ = E2 95 97
                 * ║ = E2 95 91
                 * ╔ = E2 95 94
                 * ═ = E2 95 90
                 * ╚ = E2 95 9A
                 * ╝ = E2 95 9D
                 */
                if (p[1] == (char)0x96 && p[2] == (char)0x88) {
                    /* Full block */
                    for (int cy = 0; cy < 16; cy++) {
                        uint32_t *row = fb_mem + ((cur_y + cy) * line_length / 4);
                        for (int cx = 0; cx < 8; cx++) row[cur_x + cx] = COLOR_BRAND;
                    }
                } else if (p[1] == (char)0x95) {
                    /* Line drawing chars: for Stage 0, just draw them as partial blocks */
                    char t = p[2];
                    int start_x = (t == (char)0x97 || t == (char)0x9D) ? 0 : 2;
                    int end_x   = (t == (char)0x94 || t == (char)0x9A) ? 8 : 6;
                    int start_y = (t == (char)0x9A || t == (char)0x9D) ? 0 : 4;
                    int end_y   = (t == (char)0x97 || t == (char)0x94) ? 16 : 12;
                    
                    if (t == (char)0x90) { start_y = 6; end_y = 10; start_x = 0; end_x = 8; } /* ═ */
                    if (t == (char)0x91) { start_x = 2; end_x = 6; start_y = 0; end_y = 16; } /* ║ */
                    
                    for (int cy = start_y; cy < end_y; cy++) {
                        uint32_t *row = fb_mem + ((cur_y + cy) * line_length / 4);
                        for (int cx = start_x; cx < end_x; cx++) row[cur_x + cx] = COLOR_BRAND;
                    }
                }
                cur_x += 8;
                p += 3;
            } else {
                p++;
            }
        }
    }
    
    render_text_centered(center_y + 120, "Initializing System", COLOR_TEXT);
}

void render_progress(const char *msg, int percent) {
    int center_y = screen_h / 2 + 20;
    
    /* Clear the area first */
    for (int y = center_y; y < center_y + 60; y++) {
        uint32_t *row = fb_mem + (y * line_length / 4);
        for (int x = 0; x < screen_w; x++) {
            row[x] = COLOR_BG;
        }
    }
    
    render_text_centered(center_y, msg, COLOR_TEXT);
    
    /* Draw progress bar */
    int bar_w = 400;
    int bar_h = 10;
    int bar_x = (screen_w - bar_w) / 2;
    int bar_y = center_y + 30;
    
    for (int y = bar_y; y < bar_y + bar_h; y++) {
        uint32_t *row = fb_mem + (y * line_length / 4);
        for (int x = bar_x; x < bar_x + bar_w; x++) {
            row[x] = COLOR_BAR_BG;
        }
    }
    
    int fill_w = (bar_w * percent) / 100;
    for (int y = bar_y; y < bar_y + bar_h; y++) {
        uint32_t *row = fb_mem + (y * line_length / 4);
        for (int x = bar_x; x < bar_x + fill_w; x++) {
            row[x] = COLOR_BRAND;
        }
    }
}
