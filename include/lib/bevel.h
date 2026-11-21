#ifndef __LIB_BEVEL_H
#define __LIB_BEVEL_H

/**
 * Raised and sunken parts, as Windows 95 drew them: the Editor's header bar and buttons, and scroll bars
 * (lib/scrollbar.h). They're drawn through a fill function, so apps and the kernel share them: apps fill with
 * gfx_draw_rectangle (gfx_draw_bevel, scrollbar_draw in lib/graphics.c), and the kernel fills a window of its own, as
 * the Terminal does its scroll bar.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <colors.h>

/* A face, a light edge and a shadow, with black, in Graphics.hpp's drawHeaderTable colors */
#define COLOR_BAR_FACE 30
#define COLOR_BAR_LIGHT COLOR_VGA_LIGHTEST_GRAY
#define COLOR_BAR_SHADOW (COLOR_VGA_MEDIUM_DARK_GRAY + 5)

/* Fills a rectangle with color in what context draws into */
typedef void (*paint_fill_t)(void* context, int x, int y, int width, int height, unsigned char color);

/**
 * Draws a raised part, like a button, or with pressed set, a sunken one: the face, light top and left edges and black
 * bottom and right ones, the other way around when sunken, and a shadow inside the black.
 */
void bevel_paint(paint_fill_t fill, void* context, int x, int y, int width, int height, int pressed);

#ifdef __cplusplus
}
#endif

#endif /* __LIB_BEVEL_H */
