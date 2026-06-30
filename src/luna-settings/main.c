/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * luna-settings — MahinaOS System Settings application.
 *
 * Pages:
 *   0. Home (category list)
 *   1. Display
 *   2. Network (placeholder — NetworkManager integration TBD)
 *   3. Audio
 *   4. Users & Accounts
 *   5. About System
 *
 * Each category is a button on the home page. Clicking navigates to that
 * settings panel. A "← Back" button returns to the home page.
 */

#include "lunagui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static lgui_application_t *g_app  = NULL;
static lgui_window_t      *g_win  = NULL;
static lgui_widget_t      *g_root = NULL;
static int                 g_page = 0;

static void render_page(int page);

/* ── Navigation ───────────────────────────────────────────────────────────── */

static void btn_back(lgui_widget_t *b, void *u)  { (void)b;(void)u; render_page(0); }
static void btn_p1(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(1); }
static void btn_p2(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(2); }
static void btn_p3(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(3); }
static void btn_p4(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(4); }
static void btn_p5(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(5); }
static void btn_p6(lgui_widget_t *b, void *u)    { (void)b;(void)u; render_page(6); }

/* ── Page builders ────────────────────────────────────────────────────────── */

static lgui_widget_t *make_back_row(void) {
    lgui_widget_t *h = lgui_hbox_create();
    lgui_widget_set_size(h, 800, 40);
    lgui_widget_t *b = lgui_button_create("← Settings");
    lgui_button_set_on_click(b, btn_back, NULL);
    lgui_box_add_child(h, b);
    return h;
}

static lgui_widget_t *page_home(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);

    lgui_widget_t *title = lgui_label_create("System Settings");
    lgui_widget_set_size(title, 800, 32);
    lgui_box_add_child(v, title);
    lgui_box_add_child(v, lgui_label_create(""));

    const char *cats[] = { "Display", "Wallpaper", "Network", "Audio",
                            "Users & Accounts", "About System" };
    lgui_button_click_cb cbs[] = { btn_p1, btn_p6, btn_p2, btn_p3, btn_p4, btn_p5 };
    for (int i = 0; i < 6; i++) {
        lgui_widget_t *btn = lgui_button_create(cats[i]);
        lgui_widget_set_size(btn, 400, 36);
        lgui_button_set_on_click(btn, cbs[i], NULL);
        lgui_box_add_child(v, btn);
    }
    return v;
}

static lgui_widget_t *page_display(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    lgui_box_add_child(v, lgui_label_create("Display Settings"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Resolution:"));
    lgui_box_add_child(v, lgui_button_create("1920 × 1080 (current)"));
    lgui_box_add_child(v, lgui_button_create("1280 × 720"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Refresh Rate:  60 Hz"));
    lgui_box_add_child(v, lgui_label_create("Color Depth:   32-bit XRGB8888"));
    return v;
}

static lgui_widget_t *page_network(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    lgui_box_add_child(v, lgui_label_create("Network Settings"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Status: NetworkManager not running"));
    lgui_box_add_child(v, lgui_label_create("Wired: Not detected"));
    lgui_box_add_child(v, lgui_label_create("Wireless: Not detected"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Network configuration is available"));
    lgui_box_add_child(v, lgui_label_create("via /etc/network/interfaces or nmcli."));
    return v;
}

static lgui_widget_t *page_audio(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    lgui_box_add_child(v, lgui_label_create("Audio Settings"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Output Device:  Default (ALSA)"));
    lgui_box_add_child(v, lgui_label_create("Input Device:   Default (ALSA)"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Volume: 80%"));
    lgui_box_add_child(v, lgui_button_create("Mute"));
    return v;
}

static lgui_widget_t *page_users(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    lgui_box_add_child(v, lgui_label_create("Users & Accounts"));
    lgui_box_add_child(v, lgui_label_create(""));

    /* Read current user from env */
    const char *user = getenv("USER");
    if (!user) user = "mahina";
    char buf[128];
    snprintf(buf, sizeof(buf), "Current user: %s", user);
    lgui_box_add_child(v, lgui_label_create(buf));
    lgui_box_add_child(v, lgui_label_create("Privileges: Administrator"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_button_create("Change Password"));
    lgui_box_add_child(v, lgui_button_create("Add User..."));
    return v;
}

static lgui_widget_t *page_about(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    lgui_box_add_child(v, lgui_label_create("About MahinaOS"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Version:      MahinaOS 0.1.0 (Horizon)"));
    lgui_box_add_child(v, lgui_label_create("Kernel:       Linux 6.8"));
    lgui_box_add_child(v, lgui_label_create("Compositor:   lgp-compositor 1.0"));
    lgui_box_add_child(v, lgui_label_create("GUI Toolkit:  LunaGUI 1.0"));
    lgui_box_add_child(v, lgui_label_create("Architecture: x86-64"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Copyright (c) 2026 Hardik Bhaskar"));
    lgui_box_add_child(v, lgui_label_create("Licensed under the MIT License."));
    return v;
}

/* ── Wallpaper Page ───────────────────────────────────────────────────────── */

static void copy_file(const char *src, const char *dst) {
    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) {
        printf("luna-settings: failed to open source %s\n", src);
        return;
    }
    FILE *fdst = fopen(dst, "wb");
    if (!fdst) {
        printf("luna-settings: failed to open destination %s\n", dst);
        fclose(fsrc);
        return;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        fwrite(buf, 1, n, fdst);
    }
    fclose(fsrc);
    fclose(fdst);
}

static void btn_select_wp_synthwave(lgui_widget_t *b, void *u) {
    (void)b; (void)u;
    copy_file("/usr/share/wallpaper/synthwave.lraw", "/usr/share/wallpaper/live.lraw");
    printf("luna-settings: Selected Synthwave Sunrise wallpaper\n");
}

static void btn_select_wp_retro_lake(lgui_widget_t *b, void *u) {
    (void)b; (void)u;
    copy_file("/usr/share/wallpaper/retro_lake.lraw", "/usr/share/wallpaper/live.lraw");
    printf("luna-settings: Selected Retro Lake at Night wallpaper\n");
}

static lgui_widget_t *page_wallpaper(void) {
    lgui_widget_t *v = lgui_vbox_create();
    lgui_widget_set_size(v, 800, 600);
    lgui_box_add_child(v, make_back_row());
    
    lgui_box_add_child(v, lgui_label_create("Wallpaper Manager"));
    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Select looping video wallpaper:"));
    
    lgui_widget_t *btn1 = lgui_button_create("Synthwave Sunrise (Default Loop)");
    lgui_button_set_on_click(btn1, btn_select_wp_synthwave, NULL);
    lgui_box_add_child(v, btn1);

    lgui_widget_t *btn2 = lgui_button_create("Retro Lake at Night (Custom Video Loop)");
    lgui_button_set_on_click(btn2, btn_select_wp_retro_lake, NULL);
    lgui_box_add_child(v, btn2);

    lgui_box_add_child(v, lgui_label_create(""));
    lgui_box_add_child(v, lgui_label_create("Wallpaper is loaded dynamically by the shell background."));
    return v;
}

/* ── Router ───────────────────────────────────────────────────────────────── */

typedef lgui_widget_t *(*page_fn_t)(void);
static const page_fn_t pages[] = {
    page_home, page_display, page_network,
    page_audio, page_users, page_about, page_wallpaper
};

static void render_page(int page) {
    if (g_root) { lgui_widget_destroy(g_root); g_root = NULL; }
    g_page = page;
    int n = (int)(sizeof(pages) / sizeof(pages[0]));
    g_root = pages[(page >= 0 && page < n) ? page : 0]();
    lgui_window_set_root_widget(g_win, g_root);
}

/* ── Entry ────────────────────────────────────────────────────────────────── */

int main(void) {
    g_app = lgui_application_create("luna-settings");
    if (!g_app) return 1;
    g_win = lgui_window_create(g_app, 800, 600, LGUI_LAYER_APPLICATION);
    if (!g_win) { lgui_application_destroy(g_app); return 1; }
    render_page(0);
    lgui_window_show(g_win);
    lgui_application_run(g_app);
    lgui_application_destroy(g_app);
    return 0;
}
