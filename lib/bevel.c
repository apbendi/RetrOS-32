/**
 * @file bevel.c
 * @brief Raised and sunken parts (lib/bevel.h), built into the kernel and libgraphic.
 */

#include <lib/bevel.h>

void bevel_paint(paint_fill_t fill, void* context, int x, int y, int width, int height, int pressed)
{
    unsigned char top_left = pressed ? COLOR_BLACK : COLOR_BAR_LIGHT;
    unsigned char bottom_right = pressed ? COLOR_BAR_LIGHT : COLOR_BLACK;

    fill(context, x, y, width, height, COLOR_BAR_FACE);
    fill(context, x, y, width, 1, top_left);
    fill(context, x, y, 1, height, top_left);
    fill(context, x, y + height - 1, width, 1, bottom_right);
    fill(context, x + width - 1, y, 1, height, bottom_right);

    /* A shadow inside the dark edges: the bottom and right when raised, the top and left when pressed */
    if (pressed) {
        fill(context, x + 1, y + 1, width - 2, 1, COLOR_BAR_SHADOW);
        fill(context, x + 1, y + 1, 1, height - 2, COLOR_BAR_SHADOW);
    } else {
        fill(context, x + 1, y + height - 2, width - 2, 1, COLOR_BAR_SHADOW);
        fill(context, x + width - 2, y + 1, 1, height - 2, COLOR_BAR_SHADOW);
    }
}
