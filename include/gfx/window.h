#ifndef __WINDOW_H
#define __WINDOW_H

#include <stdint.h>
#include <gfx/events.h>
struct window;
#include <gfx/component.h>
#include <terminal.h>
#include <pcb.h>
#include <colors.h>
#include <sync.h>
#include <kutils.h>

#define GFX_MAX_WINDOW_NAME_SIZE 20
#define GFX_WINDOW_BG_COLOR COLOR_BOX_GRAY_DEFAULT
#define GFX_WINDOW_TITLE_HEIGHT 12
/* The title bar and borders are drawn this many pixels above window->y */
#define GFX_WINDOW_HEADER_OVERHANG 4
/* Smallest size a resize allows, leaving room for the title bar's buttons and drag area */
#define GFX_WINDOW_MIN_WIDTH 128
#define GFX_WINDOW_MIN_HEIGHT 48
#define GFX_MAX_EVENTS 10

#define WINDOW_GET_PIXEL(w, x, y) w->inner[x + y * w->pitch]

typedef enum window_states {
    GFX_WINDOW_MOVING,
    GFX_WINDOW_STATIC
} window_state_t;

typedef enum window_flags {
    GFX_IS_RESIZABLE = 1 << 0,
    GFX_IS_IMMUATABLE = 1 << 1,
    GFX_IS_MOVABLE = 1 << 2,
    GFX_IS_TRANSPARENT = 1 << 3,
    GFX_IS_HIDDEN = 1 << 4,
    GFX_HIDE_HEADER = 1 << 5,
    GFX_HIDE_BORDER = 1 << 6,
    GFX_NO_OPTIONS = 1 << 7,
} window_flag_t;

/* window ops */
struct window_ops {
    void (*click)(struct window*, int x, int y);
    void (*hover)(struct window*, int x, int y);
    void (*mousedown)(struct window*, int x, int y);
    void (*mouseup)(struct window*, int x, int y);
    int (*resize)(struct window*, int width, int height);
    void (*move)(struct window*, int x, int y);
    int (*destroy)(struct window*);
    int (*maximize)(struct window*);
};

/* window draw ops */
struct window_draw_ops {
    void (*draw)(uint8_t*, struct window*);
    int (*rect)(struct window*, int x, int y, int width, int height, color_t color);
    int (*textf)(struct window*, int x, int y, color_t color, const char* fmt, ...);
    int (*text)(struct window*, int x, int y, const char* text, color_t color);
    void (*line)(struct window*, int x1, int y1, int x2, int y2, color_t color);
    void (*circle)(struct window*, int x, int y, int radius, color_t color, bool_t fill);
    int (*box)(struct window*, int x, int y, int width, int height, color_t color);
    int (*bitmap)(struct window*, int x, int y, int width, int height, uint8_t* bitmap);
};

struct window {
    struct window* next;

    char name[GFX_MAX_WINDOW_NAME_SIZE];
    char header[GFX_MAX_WINDOW_NAME_SIZE];

    /* TODO: create proper buttons */
    char buttons[5][GFX_MAX_WINDOW_NAME_SIZE];
    
    uint16_t x, y;
    uint16_t width, height, inner_width, inner_height;
    uint16_t pitch;

    /* draw ops */
    struct window_draw_ops* draw;

    /* ops */
    struct window_ops* ops;

    struct {
        struct gfx_event list[GFX_MAX_EVENTS];
        uint8_t head;
        uint8_t tail;
    } events;

    struct {
        uint8_t border;
        uint8_t header;
        uint8_t text;
    } color;

    /* While moving, where the window was grabbed, relative to x and y. y is negative on the frame drawn above y. */
    struct {
        char state;
        int16_t x;
        int16_t y;
    } is_moving; 

    /* Pointer to inner memory where applications draw. */
    uint8_t* inner;
    uint8_t is_resizable;
    uint8_t resize;    

    /* Content size the outline shows during a resize. The buffer is only resized to it on release. */
    struct {
        uint16_t width;
        uint16_t height;
    } pending_size;

    struct pcb* owner;
    spinlock_t spinlock;

    /**
     * Held while drawing into inner or replacing it. Drawing runs on the owner's thread and resizing
     * on the window server's, so this keeps a resize from swapping the buffer in the middle of a draw.
     */
    spinlock_t buffer_lock;

    byte_t in_focus;
    
    struct {
        uint16_t width;
        uint16_t height;
        byte_t state;
    } is_maximized;

    unsigned char flags;
    char changed;
};


struct window* gfx_new_window(int width, int height, window_flag_t flags);

void gfx_draw_window(uint8_t* buffer, struct window* window);
int gfx_destroy_window(struct window* w);
void gfx_window_set_resizable();


#endif //  __WINDOW_H   
