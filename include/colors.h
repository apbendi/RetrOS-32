#ifndef D0F51827_3250_49B2_9614_254FFF11BBC3
#define D0F51827_3250_49B2_9614_254FFF11BBC3


typedef unsigned char color_t;

/* Possible background colors 18, 41, 45 */

#define COLOR_TURQUISE 50 // 50

enum grays {
    COLOR_VGA_DARKEST_GRAY = 17,
    COLOR_VGA_DARK_GRAY = 19,
    COLOR_VGA_MEDIUM_DARK_GRAY = 21,
    COLOR_VGA_MEDIUM_GRAY = 23,
    COLOR_VGA_MEDIUM_LIGHT_GRAY = 25,
    COLOR_VGA_LIGHT_GRAY = 27,
    COLOR_VGA_LIGHTER_GRAY = 29,
    COLOR_VGA_LIGHTEST_GRAY = 31
};

/**
 * Transparent: not drawn in icons, or in a GFX_IS_TRANSPARENT window's content. Like the palette's other entries from
 * 248, it's black otherwise, which index 0 already gives
 */
#define COLOR_TRANSPARENT 0xFF

#define COLOR_VGA_BG 18
#define COLOR_VGA_FG 90
#define COLOR_VGA_MISC 125
#define COLOR_VGA_RED 4
#define COLOR_VGA_GREEN 0x2
#define COLOR_VGA_PURPLE 0x3b
#define COLOR_VGA_YELLOW 0x44
#define COLOR_VGA_LIGHT_BLUE 0x36

/* Black and white */
#define COLOR_BLACK 0x00
#define COLOR_WHITE 0x0F

#endif /* D0F51827_3250_49B2_9614_254FFF11BBC3 */
