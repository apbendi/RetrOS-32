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

    /**
     * What the framebuffer shows: the composition buffer as last copied, with the cursor over it where its top left
     * corner was then, at _screen_cursor_x and _screen_cursor_y. Copying compares with this rather than reading the
     * framebuffer back, which is slow on a real card (#163).
     */
    uint8_t* _screen;
    int _screen_cursor_x, _screen_cursor_y;

    /* A screencheck asked for (ws_check_screen): the buffer to composite into, and the result once done is set */
    struct {
        uint8_t* buffer;
        int done;
        int result;
        struct gfx_rect diff;
    } _check;

    /* The desktop's color, behind the windows */
    color_t background;

    int buffer_size;

    int workspace;

    struct windowmanager* _wm;
    struct window_server_ops* ops;

    /* states */
    struct mouse m;
    /* Asks for the screen to be composited again, from any thread. ws_draw takes it outside fullscreen. */
    int window_changes;
    
    byte_t flags;
};

/* window server functions */
struct windowserver* ws_new();
int ws_check_screen(struct windowserver* ws, struct gfx_rect* diff);



#endif /* !WINDOW_SERVER_H */