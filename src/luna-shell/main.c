/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>

#include "../../luna-gui/include/lunagui.h"

#define MAX_SURFACES 64
#define LGP_MOD_ALT   0x04
#define LGP_MOD_SUPER 0x08

typedef struct {
    uint32_t id;
    uint32_t type;
    int x;
    int y;
} shell_surface_t;

static struct {
    lgui_application_t *app;
    lgui_window_t *topbar;
    lgui_window_t *wallpaper;
    
    shell_surface_t surfaces[MAX_SURFACES];
    size_t surface_count;
    
    int next_x;
    int next_y;
} shell;

static void launch_terminal(void) {
    if (fork() == 0) {
        /* In child process */
        char *argv[] = {"luna-terminal", NULL};
        execvp("luna-terminal", argv);
        /* If execvp fails, we exit */
        exit(1);
    }
}

static void render_wallpaper(lgui_widget_t *widget, lgui_canvas_t *canvas, int ox, int oy) {
    /* Draw a simple gradient or solid color */
    lgui_canvas_fill_rect(canvas, 0, 0, 1024, 768, 0xFF2A2A35); /* Dark blue/grey */
}

static void on_surface_created(uint32_t surface_id, uint32_t type, uint32_t w, uint32_t h, void *user_data) {
    if (type != 4) return; /* LGP_SURFACE_APPLICATION_WINDOW */
    
    if (shell.surface_count < MAX_SURFACES) {
        shell.surfaces[shell.surface_count].id = surface_id;
        shell.surfaces[shell.surface_count].type = type;
        
        /* Cascading placement */
        shell.surfaces[shell.surface_count].x = shell.next_x;
        shell.surfaces[shell.surface_count].y = shell.next_y;
        
        lgui_wm_set_surface_position(shell.app, surface_id, shell.next_x, shell.next_y);
        lgui_wm_set_focus(shell.app, surface_id);
        
        shell.next_x += 30;
        shell.next_y += 30;
        if (shell.next_x > 400) {
            shell.next_x = 100;
            shell.next_y = 100;
        }
        
        shell.surface_count++;
    }
}

static void on_surface_destroyed(uint32_t surface_id, void *user_data) {
    for (size_t i = 0; i < shell.surface_count; i++) {
        if (shell.surfaces[i].id == surface_id) {
            /* Remove by swapping with last */
            shell.surfaces[i] = shell.surfaces[shell.surface_count - 1];
            shell.surface_count--;
            break;
        }
    }
}

static void on_global_key(uint32_t key, uint32_t modifiers, void *user_data) {
    if (key == KEY_T && (modifiers & LGP_MOD_SUPER)) {
        printf("luna-shell: Launching terminal\n");
        launch_terminal();
    } else if (key == KEY_TAB && (modifiers & LGP_MOD_ALT)) {
        printf("luna-shell: Alt+Tab pressed\n");
        if (shell.surface_count > 1) {
            /* Cycle focus to the next surface */
            shell_surface_t first = shell.surfaces[0];
            for (size_t i = 0; i < shell.surface_count - 1; i++) {
                shell.surfaces[i] = shell.surfaces[i + 1];
            }
            shell.surfaces[shell.surface_count - 1] = first;
            
            /* The newly focused surface is now at the end */
            lgui_wm_set_focus(shell.app, shell.surfaces[shell.surface_count - 1].id);
        }
    }
}

int main(int argc, char **argv) {
    printf("Starting luna-shell...\n");
    
    memset(&shell, 0, sizeof(shell));
    shell.next_x = 100;
    shell.next_y = 100;
    
    shell.app = lgui_application_create_wm("luna-shell", on_surface_created, on_surface_destroyed, NULL);
    if (!shell.app) {
        fprintf(stderr, "luna-shell: failed to connect as WM\n");
        return 1;
    }
    
    lgui_application_set_global_key_cb(shell.app, on_global_key, NULL);
    
    /* Register keyboard grabs */
    lgui_wm_grab_key(shell.app, KEY_T, LGP_MOD_SUPER);
    lgui_wm_grab_key(shell.app, KEY_TAB, LGP_MOD_ALT);
    
    /* Create Wallpaper */
    shell.wallpaper = lgui_window_create(shell.app, 1024, 768, LGUI_LAYER_WALLPAPER);
    lgui_widget_t *wp_canvas = lgui_canvas_widget_create();
    lgui_canvas_widget_set_render(wp_canvas, render_wallpaper);
    lgui_window_set_root_widget(shell.wallpaper, wp_canvas);
    lgui_window_show(shell.wallpaper);
    
    /* Create Top Bar */
    shell.topbar = lgui_window_create(shell.app, 1024, 32, LGUI_LAYER_SHELL);
    lgui_widget_t *hbox = lgui_hbox_create();
    lgui_widget_t *logo = lgui_label_create("  MahinaOS  ");
    lgui_widget_t *time = lgui_label_create(" 12:00 ");
    lgui_box_add_child(hbox, logo);
    lgui_box_add_child(hbox, time);
    lgui_window_set_root_widget(shell.topbar, hbox);
    lgui_window_show(shell.topbar);
    
    printf("luna-shell: Initialization complete. Running event loop.\n");
    lgui_application_run(shell.app);
    
    lgui_application_destroy(shell.app);
    return 0;
}
