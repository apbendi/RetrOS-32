#ifndef D05A48F1_86E3_4F50_8DF8_56BA8BEC696B
#define D05A48F1_86E3_4F50_8DF8_56BA8BEC696B

#include <stdint.h>
#include <keyboard.h>

/* Key codes for apps, from keyboard.h, which also has KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN and KEY_DELETE */
#define KEY_UP ARROW_UP
#define KEY_DOWN ARROW_DOWN
#define KEY_LEFT ARROW_LEFT
#define KEY_RIGHT ARROW_RIGHT
#define KEY_F9 F9
#define KEY_F4 F4
#define KEY_F3 F3
#define KEY_F2 F2
#define KEY_F1 F1

typedef enum gfx_event_flags {
    GFX_EVENT_BLOCKING = 1 << 0,
    GFX_EVENT_NONBLOCKING = 1 << 1
} gfx_event_flag_t;

typedef enum gfx_events {
    GFX_EVENT_KEYBOARD,
    GFX_EVENT_MOUSE,
    GFX_EVENT_EXIT,
    GFX_EVENT_RESOLUTION
} gfx_event_t;

/**
 * flags is the modifier keys held, KEY_MOD_SHIFT, KEY_MOD_CTRL and KEY_MOD_ALT from keyboard.h, on a keyboard event:
 * those held when the key was pressed. It's 0 on other events. It fills the padding after event, so the struct is
 * 6 bytes, as it was before flags.
 */
struct gfx_event {
    uint8_t event;
    uint8_t flags;
    uint16_t data;
    uint16_t data2;
};

struct gfx_event_key {
    uint8_t event;
    uint8_t flags;
    uint16_t data;
    uint16_t unused;
};

struct gfx_event_mouse {
    uint8_t event;
    uint8_t flags;
    uint16_t x;
    uint16_t y;
};

#endif /* D05A48F1_86E3_4F50_8DF8_56BA8BEC696B */
