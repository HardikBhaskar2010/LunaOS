#ifndef MAHINA_LGP_COMPOSITOR_MAIN_H
#define MAHINA_LGP_COMPOSITOR_MAIN_H

#include "ipc/client.h"
#include "scene/surface.h"

typedef struct {
    int epoll_fd;
    int signal_fd;
    bool running;
    uint32_t next_session_id;
    lgp_client_t *clients;
    lgp_surface_manager_t surface_manager;

    uint32_t keyboard_focus_session_id;
    uint32_t keyboard_focus_surface_id;
    char global_clipboard[4096];
    
    struct {
        uint32_t key;
        uint32_t modifiers;
    } grabbed_keys[16];
    size_t grabbed_keys_count;
    
    lgp_client_t *wm_client;
} lgp_compositor_state_t;

void lgp_dispatch_pointer_motion(lgp_compositor_state_t *state, int x, int y);
void lgp_dispatch_pointer_button(lgp_compositor_state_t *state, int x, int y, uint8_t button, bool pressed);
void lgp_dispatch_keyboard_key(lgp_compositor_state_t *state, uint32_t key, uint32_t key_state, uint32_t modifiers);

#endif
