#ifndef __GRAPHICS_H
#define __GRAPHICS_H

/**
 * @brief Library version of kernel gfxlib
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#include <lib/syscall.h>
#include <colors.h>
#include <args.h>

#endif

#include <gfx/events.h>
#include <image.h>

enum gfx_lib_api_options {
    GFX_DRAW_CHAR_OPT,
    GFX_DRAW_TEXT_OPT,
    GFX_PUT_CHAR_OPT,
    GFX_DRAW_RECTANGLE_OPT,
    GFX_DRAW_CIRCLE_OPT,
    GFX_DRAW_LINE_OPT,
    GFX_EVEN_LOOP_OPT,
    GFX_DRAW_PIXEL,
    GFX_DRAW_IMAGE,
    GFX_DRAW_UNUSED,
};

/* The largest width or height of an image an app draws */
#define GFX_IMAGE_MAX_SIDE 1024

/* An image to draw at x, y: its pixels are in the app's memory */
struct gfx_image {
    int x, y;
    struct image image;
};

struct gfx_pixel {
    int x, y;
    unsigned char color;
};

struct gfx_rectangle {
    int x, y, width, height;
    unsigned char color;
};

struct gfx_circle {
    int x, y, r;
    char fill;
    unsigned char color;
};

struct gfx_line {
    int x0, y0, x1, y1;
    unsigned char color;
};

#define GFX_TEXT_MAX 100
struct gfx_text {
    int x, y;
    char data[GFX_TEXT_MAX];
    unsigned char color;
};

struct gfx_char {
    int x, y;
    char data;
    unsigned char color;
};

int gfx_draw_text(int x, int y, const char* text, unsigned char color);
int gfx_draw_format_text(int x, int y, unsigned char color, const char* fmt, ...);
int gfx_draw_char(int x, int y, char data, unsigned char color);
int gfx_draw_rectangle(int x, int y, int width, int height, unsigned char color);
int gfx_draw_circle(int x, int y, int r, unsigned char color, char fill);
int gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color);
int gfx_get_event(struct gfx_event*, gfx_event_flag_t flags);
int gfx_draw_pixel(int x, int y, unsigned char color);
/**
 * Draws an image in one call, leaving the window as it is where the image is COLOR_TRANSPARENT.
 * @return 0, or a negative error code
 */
int gfx_draw_image(int x, int y, const struct image* image);

/**
 * Loads ICONS_DIR NAME.bmp, an image the kernel and apps share, into image, with its pixels malloc'd. If it's missing
 * or can't be read, image is a 16x16 placeholder instead, a box with a red X, which gfx_free_icon leaves alone.
 * @return 0, or a negative error code, when image is the placeholder
 */
int gfx_load_icon(const char* name, struct image* image);
void gfx_free_icon(struct image* image);


#ifdef __cplusplus
}
#endif

#endif /* __GRAPHICS_H */