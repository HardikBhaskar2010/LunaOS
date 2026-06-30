/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 *
 * luna-shell — MahinaOS Desktop Shell & Window Manager
 *
 * "Luna Island" desktop:
 *   - WALLPAPER surface: animated gradient + star field (software rendered)
 *   - SHELL overlay: top panel, left dock, window decorations
 *   - WM role: manages surface positions, focus, drag
 *
 * Architecture:
 *   lgui_application_create_wm() → WM capabilities, gets surface events
 *   Two windows:
 *     shell.wallpaper  (LGUI_LAYER_WALLPAPER): full-screen animated bg
 *     shell.overlay    (LGUI_LAYER_SHELL):     top panel + dock + decorations
 *
 * Animation driven by the poll() 16ms timeout in lgui_application_run()
 * via a registered timerfd.
 *
 * Key bindings:
 *   Super+T    → launch luna-terminal
 *   Super+F    → launch luna-files
 *   Alt+Tab    → cycle window focus
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>
#include <stdint.h>
#include <linux/input.h>

#include "../../luna-gui/include/lunagui.h"

#include "wallpaper.h"
#include "dock.h"
#include "decorations.h"
#include "topbar.h"

/* ── Key modifier constants ─────────────────────────────────────────────── */
#define LGP_MOD_SHIFT  0x01
#define LGP_MOD_CTRL   0x02
#define LGP_MOD_ALT    0x04
#define LGP_MOD_SUPER  0x08

/* ── Shell global state ─────────────────────────────────────────────────── */

static struct {
    lgui_application_t *app;
    lgui_window_t      *wallpaper_win;
    lgui_window_t      *overlay_win;

    uint32_t screen_w;
    uint32_t screen_h;

    wallpaper_state_t  wp;
    topbar_state_t     tb;
    dock_state_t       dk;
    deco_state_t       dc;

    /* Managed application surfaces */
    uint32_t           surfaces[64];
    int                surface_count;
    int                focus_idx;   /* Index into surfaces[] for current focus */

    /* Timer fd for animation */
    int                timer_fd;

    /* Pointer state (for dock hover + drag) */
    int                ptr_x;
    int                ptr_y;
    bool               ptr_down;
} shell;

/* ── Helper: launch a binary ────────────────────────────────────────────── */

static void launch(const char *cmd) {
    if (fork() == 0) {
        char *argv[] = { (char *)cmd, NULL };
        execvp(cmd, argv);
        _exit(1);
    }
}

/* ── WM callbacks ───────────────────────────────────────────────────────── */

static void on_surface_created(uint32_t surface_id, uint32_t type,
                                uint32_t w, uint32_t h, void *user_data) {
    (void)user_data;

    /* Only manage APPLICATION_WINDOW surfaces (type 4) */
    if (type != 4) return;

    if (shell.surface_count < 64) {
        shell.surfaces[shell.surface_count++] = surface_id;
    }

    /* Cascading placement: offset each new window */
    static int cascade_x = 80;
    static int cascade_y = 56;
    lgui_wm_set_surface_position(shell.app, surface_id, cascade_x, cascade_y);
    lgui_wm_set_focus(shell.app, surface_id);
    shell.focus_idx = shell.surface_count - 1;

    deco_add_window(&shell.dc, surface_id, w, h);
    deco_update_position(&shell.dc, surface_id, cascade_x, cascade_y);
    deco_set_focused(&shell.dc, surface_id);

    cascade_x += 32;
    cascade_y += 32;
    if (cascade_x > 400 || cascade_y > 400) {
        cascade_x = 80;
        cascade_y = 56;
    }

    /* Force overlay repaint */
    if (shell.overlay_win) {
        lgui_window_update(shell.overlay_win);
    }
}

static void on_surface_destroyed(uint32_t surface_id, void *user_data) {
    (void)user_data;

    deco_remove_window(&shell.dc, surface_id);

    for (int i = 0; i < shell.surface_count; i++) {
        if (shell.surfaces[i] == surface_id) {
            shell.surfaces[i] = shell.surfaces[shell.surface_count - 1];
            shell.surface_count--;
            if (shell.focus_idx >= shell.surface_count)
                shell.focus_idx = shell.surface_count - 1;
            break;
        }
    }

    if (shell.overlay_win) {
        lgui_window_update(shell.overlay_win);
    }
}

/* ── Render callbacks ───────────────────────────────────────────────────── */

static void render_wallpaper(lgui_widget_t *widget, lgui_canvas_t *canvas,
                              int ox, int oy) {
    (void)widget;
    (void)ox;
    (void)oy;
    (void)canvas;
    /* Wallpaper is rendered directly into the pixel buffer in on_timer().
     * This callback is called once on first show() — do an initial render. */
    void *px = lgui_window_get_pixels(shell.wallpaper_win);
    if (px) {
        wallpaper_render(&shell.wp, px, shell.screen_w * 4u);
    }
}

static void render_overlay(lgui_widget_t *widget, lgui_canvas_t *canvas,
                            int ox, int oy) {
    (void)widget;
    (void)ox;
    (void)oy;

    uint32_t sw = shell.screen_w;
    uint32_t sh = shell.screen_h;

    /* Clear overlay to transparent */
    lgui_canvas_fill_rect(canvas, 0, 0, (int)sw, (int)sh, 0x00000000u);

    /* Top bar */
    topbar_render(&shell.tb, canvas, sw);

    /* Left dock */
    dock_render(&shell.dk, canvas, sh);

    /* Window decorations */
    deco_render(&shell.dc, canvas);
}

/* ── Timer callback (animation tick) ───────────────────────────────────── */

static void on_timer(int fd, void *user_data) {
    (void)user_data;
    uint64_t expirations = 0;
    ssize_t r = read(fd, &expirations, sizeof(expirations));
    (void)r;

    /* Update animation state */
    wallpaper_tick(&shell.wp);
    topbar_tick(&shell.tb);

    /* Redraw wallpaper directly into its pixel buffer */
    if (shell.wallpaper_win) {
        void *px = lgui_window_get_pixels(shell.wallpaper_win);
        if (px) {
            wallpaper_render(&shell.wp, px, shell.screen_w * 4u);
        }
        lgui_window_update(shell.wallpaper_win);
    }

    /* Redraw overlay into canvas via helper */
    if (shell.overlay_win) {
        lgui_canvas_t *oc = lgui_window_get_canvas(shell.overlay_win);
        if (oc) {
            uint32_t sw = shell.screen_w;
            uint32_t sh = shell.screen_h;

            /* Clear to transparent so shell composites over wallpaper */
            lgui_canvas_fill_rect(oc, 0, 0, (int)sw, (int)sh, 0x00000000u);

            /* Top panel */
            topbar_render(&shell.tb, oc, sw);

            /* Left dock */
            dock_render(&shell.dk, oc, sh);

            /* Window decorations */
            deco_render(&shell.dc, oc);
        }
        lgui_window_update(shell.overlay_win);
    }
}

/* ── Global pointer handler ─────────────────────────────────────────────── */

static void on_global_pointer(int x, int y, bool pressed, bool button_event, void *user_data) {
    (void)user_data;

    shell.ptr_x = x;
    shell.ptr_y = y;

    if (button_event) {
        shell.ptr_down = pressed;
        if (pressed) {
            /* Try clicking the dock first */
            bool dock_clicked = dock_on_click(&shell.dk, x, y);
            if (!dock_clicked) {
                /* If not dock, try dragging window titlebars */
                deco_pointer_down(&shell.dc, x, y, shell.app);
            }
        } else {
            deco_pointer_up(&shell.dc);
        }
    } else {
        /* Motion event */
        dock_on_pointer(&shell.dk, x, y);
        if (shell.ptr_down) {
            deco_pointer_move(&shell.dc, x, y, shell.app);
        }
    }

    /* Force overlay window redraw on any pointer interaction */
    if (shell.overlay_win) {
        lgui_window_update(shell.overlay_win);
    }
}

/* ── Global key handler ─────────────────────────────────────────────────── */

static void on_global_key(uint32_t key, uint32_t modifiers, void *user_data) {
    (void)user_data;

    /* Super+T → Terminal */
    if (key == KEY_T && (modifiers & LGP_MOD_SUPER)) {
        launch("luna-terminal");
        return;
    }

    /* Super+F → Files */
    if (key == KEY_F && (modifiers & LGP_MOD_SUPER)) {
        launch("luna-files");
        return;
    }

    /* Super+S → Settings */
    if (key == KEY_S && (modifiers & LGP_MOD_SUPER)) {
        launch("luna-settings");
        return;
    }

    /* Alt+Tab → cycle focus */
    if (key == KEY_TAB && (modifiers & LGP_MOD_ALT)) {
        if (shell.surface_count > 1) {
            shell.focus_idx = (shell.focus_idx + 1) % shell.surface_count;
            uint32_t sid = shell.surfaces[shell.focus_idx];
            lgui_wm_set_focus(shell.app, sid);
            deco_set_focused(&shell.dc, sid);
        }
        return;
    }
}

/* ── Entry point ────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("luna-shell: starting Luna Island desktop...\n");

    memset(&shell, 0, sizeof(shell));
    shell.focus_idx = -1;
    shell.timer_fd  = -1;

    /* Connect as Window Manager */
    shell.app = lgui_application_create_wm(
        "luna-shell",
        on_surface_created,
        on_surface_destroyed,
        NULL
    );
    if (!shell.app) {
        fprintf(stderr, "luna-shell: failed to connect as WM\n");
        return 1;
    }

    lgui_application_set_global_key_cb(shell.app, on_global_key, NULL);
    lgui_application_set_global_pointer_cb(shell.app, on_global_pointer, NULL);

    /* Grab keyboard shortcuts */
    lgui_wm_grab_key(shell.app, KEY_T,   LGP_MOD_SUPER);
    lgui_wm_grab_key(shell.app, KEY_F,   LGP_MOD_SUPER);
    lgui_wm_grab_key(shell.app, KEY_S,   LGP_MOD_SUPER);
    lgui_wm_grab_key(shell.app, KEY_TAB, LGP_MOD_ALT);

    /* Get screen dimensions */
    shell.screen_w = lgui_output_width(shell.app);
    shell.screen_h = lgui_output_height(shell.app);
    if (shell.screen_w == 0 || shell.screen_h == 0) {
        shell.screen_w = 1024;
        shell.screen_h = 768;
    }

    printf("luna-shell: display %ux%u\n", shell.screen_w, shell.screen_h);

    /* Initialise subsystems */
    wallpaper_init(&shell.wp, shell.screen_w, shell.screen_h);
    topbar_init(&shell.tb);
    dock_init(&shell.dk, shell.screen_h);
    deco_init(&shell.dc);

    /* Create wallpaper surface */
    shell.wallpaper_win = lgui_window_create(
        shell.app, shell.screen_w, shell.screen_h, LGUI_LAYER_WALLPAPER);
    if (!shell.wallpaper_win) {
        fprintf(stderr, "luna-shell: failed to create wallpaper window\n");
        lgui_application_destroy(shell.app);
        return 1;
    }

    /* Set up a canvas widget that delegates to our render function */
    lgui_widget_t *wp_widget = lgui_canvas_widget_create();
    lgui_canvas_widget_set_render(wp_widget, render_wallpaper);
    lgui_window_set_root_widget(shell.wallpaper_win, wp_widget);
    lgui_window_show(shell.wallpaper_win);

    /* Create shell overlay surface */
    shell.overlay_win = lgui_window_create(
        shell.app, shell.screen_w, shell.screen_h, LGUI_LAYER_SHELL);
    if (!shell.overlay_win) {
        fprintf(stderr, "luna-shell: failed to create overlay window\n");
        lgui_application_destroy(shell.app);
        return 1;
    }

    lgui_widget_t *ov_widget = lgui_canvas_widget_create();
    lgui_canvas_widget_set_render(ov_widget, render_overlay);
    lgui_window_set_root_widget(shell.overlay_win, ov_widget);
    /* Enable ARGB so the overlay composites over the wallpaper transparently */
    lgui_window_set_argb(shell.overlay_win, true);
    lgui_window_show(shell.overlay_win);

    /* Set up animation timer: fire every 33ms (~30fps) */
    shell.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (shell.timer_fd >= 0) {
        struct itimerspec its = {
            .it_interval = { .tv_sec = 0, .tv_nsec = 33333333 }, /* 30fps */
            .it_value    = { .tv_sec = 0, .tv_nsec = 33333333 }
        };
        timerfd_settime(shell.timer_fd, 0, &its, NULL);
        lgui_application_add_fd(shell.app, shell.timer_fd, on_timer, NULL);
    }

    /* Auto-launch terminal on startup */
    printf("luna-shell: auto-launching terminal\n");
    launch("luna-terminal");

    printf("luna-shell: entering event loop\n");
    lgui_application_run(shell.app);

    if (shell.timer_fd >= 0) close(shell.timer_fd);
    wallpaper_cleanup(&shell.wp);
    lgui_application_destroy(shell.app);
    return 0;
}
