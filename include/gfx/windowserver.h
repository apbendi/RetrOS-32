#ifndef WINDOW_SERVER_H
#define WINDOW_SERVER_H

#include <stdint.h>
#include <kutils.h>

#include <gfx/window.h>
#include <windowmanager.h>

#include <mouse.h>

#define WINDOW_SERVER_SLEEP_TIME 2
#define WS_VALIDATE(ws) if((ws)->ops == NULL || (ws)->ops->add == NULL || (ws)->ops->remove == NULL || (ws)->ops->draw == NULL) { return -ERROR_OPS_CORRUPTED; }
#define WS_VALIDATE_FLAGS(ws) if((ws)->flags & ~(WINDOW_SERVER_INITIALIZED)) { return -ERROR_INVALID_ARGUMENTS; }

typedef enum window_server_flags {
    WINDOW_SERVER_UNINITIALIZED = 1 << 0,
    WINDOW_SERVER_INITIALIZED = 1 << 1
} ws_flag_t;

struct windowserver;

/* window server operations */
struct window_server_ops {
    int (*init)(struct windowserver* ws);
    int (*add)(struct windowserver* ws, struct window* window);
    int (*remove)(struct windowserver* ws, struct window* window);
    int (*fullscreen)(struct windowserver* ws, struct window* window);
    int (*set_background)(struct windowserver* ws, color_t color);
    int (*draw)(struct windowserver* ws);
    int (*destroy)(struct windowserver* ws);
};

/* window server struct */
struct windowserver {
    uint8_t sleep_time;

    /* references for memory management */
    struct kref _krefs;

    struct pcb* server;
    struct pcb* taskbar;

    /* The window F10 made fullscreen, or NULL. It draws straight into the composition buffer. */
    struct window* _fullscreen;

    /* What going fullscreen changed on that window, so leaving gives it back exactly */
    struct {
        uint8_t* inner;
        uint16_t inner_width, inner_height, pitch;
        uint16_t x, y;
    } _saved;

    /* Destroyed windows, freed at the start of the next frame, when the window server holds no window pointers */
    struct window* _free;
    /* The desktop's color, behind the windows */
    color_t background;

    int buffer_size;

    int workspace;

    struct windowmanager* _wm;
    struct window_server_ops* ops;

    /* states */
    struct mouse m;
    int window_changes;
    
    byte_t flags;
};

/* window server functions */
struct windowserver* ws_new();



#endif /* !WINDOW_SERVER_H */