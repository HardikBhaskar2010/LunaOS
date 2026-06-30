#include "lunagui.h"
#include "lgui_private.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

static uint8_t *g_font_data = NULL;
static uint32_t g_glyph_height = 16;
static uint32_t g_glyph_width = 8;

void lgui_font_init(const char *psf_path) {
    if (g_font_data) return;
    int fd = open(psf_path, O_RDONLY);
    if (fd < 0) return;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return; }
    void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return;
    
    uint8_t *buf = map;
    if (buf[0] == 0x36 && buf[1] == 0x04) {
        g_glyph_height = buf[3];
        g_font_data = buf + 4;
    }
}

void lgui_font_draw_text(lgui_canvas_t *canvas, int x, int y, const char *text, uint32_t color) {
    if (!canvas || !canvas->pixels || !text || !g_font_data) return;
    
    int clip_x = 0, clip_y = 0, clip_w = (int)canvas->width, clip_h = (int)canvas->height;
    if (canvas->clip_count > 0) {
        clip_x = canvas->clip_stack[canvas->clip_count - 1].x;
        clip_y = canvas->clip_stack[canvas->clip_count - 1].y;
        clip_w = canvas->clip_stack[canvas->clip_count - 1].w;
        clip_h = canvas->clip_stack[canvas->clip_count - 1].h;
    }

    int cx = x;
    while (*text) {
        if (*text == '\n') {
            y += (int)g_glyph_height;
            cx = x;
        } else {
            uint8_t *glyph = g_font_data + (uint8_t)(*text) * g_glyph_height;
            for (uint32_t r = 0; r < g_glyph_height; r++) {
                int py = y + (int)r;
                if (py < clip_y || py >= clip_y + clip_h) continue;

                uint8_t row = glyph[r];
                uint32_t *dst = (uint32_t *)((uint8_t *)canvas->pixels + py * canvas->stride);
                
                for (int c = 0; c < 8; c++) {
                    int px = cx + c;
                    if (px < clip_x || px >= clip_x + clip_w) continue;

                    if (row & (1 << (7 - c))) {
                        dst[px] = color;
                    }
                }
            }
            cx += (int)g_glyph_width;
        }
        text++;
    }
}
