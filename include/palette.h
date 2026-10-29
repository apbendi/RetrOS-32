#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

/**
 * The standard VGA palette: each entry's red, green and blue, from 0 to 63, as the VGA DAC takes them. Every color in
 * RetrOS, drawn or stored, is an index into it. The kernel loads it at boot (vga_load_palette), and the host tools that
 * read and write images use it too, so lib/palette.c compiles on the host as well
 */
extern const uint8_t vga_palette[256][3];

/* A palette channel, from 0 to 63, as the 0 to 255 an image file stores */
#define PALETTE_TO_8BIT(v) ((uint8_t)(((v) * 255 + 31) / 63))

/* The transparent index, COLOR_TRANSPARENT, which colors.h defines with things the host tools can't include */
#define PALETTE_TRANSPARENT 255

/**
 * The palette's color nearest red, green and blue from 0 to 255, the lowest index of those equally near. It's never
 * PALETTE_TRANSPARENT, nor the black entries before it, which index 0 already is.
 */
uint8_t palette_nearest(int red, int green, int blue);

/**
 * The color an image file lists for a palette entry, red, green and blue from 0 to 255. The transparent entry is
 * magenta, (255, 0, 255), the usual way to mark transparency in an image editor. The palette's own full magenta is
 * listed as (254, 0, 254), so it looks the same but isn't taken for transparent: its nearest color is still itself
 */
static inline void palette_file_color(int index, uint8_t rgb[3])
{
    if (index == PALETTE_TRANSPARENT) {
        rgb[0] = 255;
        rgb[1] = 0;
        rgb[2] = 255;
        return;
    }

    for (int c = 0; c < 3; c++) {
        rgb[c] = PALETTE_TO_8BIT(vga_palette[index][c]);
    }
    if (rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 255) {
        rgb[0] = 254;
        rgb[2] = 254;
    }
}

#endif /* PALETTE_H */
