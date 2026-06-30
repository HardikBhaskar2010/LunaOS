/*
 * Copyright (c) 2026 Hardik Bhaskar
 * Licensed under the MIT License.
 *
 * luna-shell/topbar.c — Top status panel implementation.
 *
 * Layout (32px tall, full screen width):
 *   [LEFT]  🌙 MahinaOS | Luna Island | WS [1]
 *   [CENTER] HH:MM:SS
 *   [RIGHT]  CPU:xx%  MEM:xx%  [Uptime]
 *
 * All values are read from /proc on each tick (every ~60 frames).
 *
 * Visual style:
 *   - Background: dark glass #0A0A14 with bottom border #E03E8A
 *   - Logo text: magenta #E03E8A
 *   - Info text: cyan #00D8FF
 *   - Clock: white #FFFFFF
 */

#include "topbar.h"

#include <stdio.h>
#include <string.h>

/* ── System info helpers ───────────────────────────────────────────────────*/

static void read_uptime_clock(char *out, size_t sz) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) { snprintf(out, sz, "00:00:00"); return; }
    double up = 0.0;
    int scanned = fscanf(f, "%lf", &up);
    fclose(f);
    if (scanned != 1) { snprintf(out, sz, "00:00:00"); return; }
    int sec = (int)up;
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    snprintf(out, sz, "%02d:%02d:%02d", h, m, s);
}

static uint32_t read_mem_percent(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0u;
    unsigned long total = 0, available = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        unsigned long val = 0;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1) total = val;
        if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) available = val;
        if (total && available) break;
    }
    fclose(f);
    if (total == 0) return 0u;
    return (uint32_t)((total - available) * 100 / total);
}

/* ── Colour palette ────────────────────────────────────────────────────────*/
#define TB_LEFT_INSET  72

#define TB_BG       0xEE0A0A14u  /* Acrylic near-black panel */
#define TB_BORDER   0xFFE03E8Au  /* Magenta bottom border */
#define TB_LOGO     0xFFE03E8Au  /* Magenta logo */
#define TB_ISLAND   0xFF8A2BE2u  /* Purple "Luna Island" */
#define TB_CLOCK    0xFFFFFFFFu  /* White clock */
#define TB_INFO     0xFF00D8FFu  /* Cyan info */
#define TB_WS       0xFFA0FFA0u  /* Green workspace indicator */

/* ── API ───────────────────────────────────────────────────────────────────*/

void topbar_init(topbar_state_t *t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->workspace = 1;
    topbar_tick(t); /* Initial read */
}

void topbar_tick(topbar_state_t *t) {
    if (!t) return;
    t->tick++;
    /* Update clock every frame */
    read_uptime_clock(t->clock_str, sizeof(t->clock_str));
    /* Update system stats every 60 ticks (~1s at 60fps) */
    if (t->tick % 60 == 0) {
        t->mem_percent = read_mem_percent();
    }
}

void topbar_render(const topbar_state_t *t, lgui_canvas_t *canvas, uint32_t sw) {
    if (!t || !canvas) return;

    int w = (int)sw;

    /* Background */
    lgui_canvas_fill_rect(canvas, 0, 0, w, TOPBAR_HEIGHT, TB_BG);

    /* Bottom border line */
    lgui_canvas_fill_rect(canvas, 0, TOPBAR_HEIGHT - 1, w, 1, TB_BORDER);

    /* ── LEFT section ─────────────────────────────────────────────────────*/
    int lx = TB_LEFT_INSET;
    lgui_canvas_draw_text(canvas, lx, 8, "MahinaOS", TB_LOGO);
    lx += 72;
    lgui_canvas_draw_text(canvas, lx, 8, "| Luna Island", TB_ISLAND);
    lx += 112;

    char ws_str[16];
    snprintf(ws_str, sizeof(ws_str), "| WS [%d]", t->workspace);
    lgui_canvas_draw_text(canvas, lx, 8, ws_str, TB_WS);

    /* ── CENTER section — clock ──────────────────────────────────────────*/
    /* Each char is 8px wide; center the clock string */
    int clock_len = (int)strlen(t->clock_str) * 8;
    int cx = (w - clock_len) / 2;
    lgui_canvas_draw_text(canvas, cx, 8, t->clock_str, TB_CLOCK);

    /* ── RIGHT section — system stats ────────────────────────────────────*/
    char info[64];
    snprintf(info, sizeof(info), "NET --  AUDIO --  MEM:%u%%", t->mem_percent);
    int info_w = (int)strlen(info) * 8;
    int rx = w - info_w - 8;
    lgui_canvas_draw_text(canvas, rx, 8, info, TB_INFO);
}
