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

#endif /* PALETTE_H */
