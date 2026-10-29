#ifndef IMAGE_H
#define IMAGE_H

/**
 * An image in memory: width x height palette indexes, row by row from the top left, with COLOR_TRANSPARENT (255) where
 * nothing is drawn. Images built into the kernel are generated from images/*.bmp (tools/bmp.c, bmp2c)
 */
struct image {
    int width;
    int height;
    const unsigned char* pixels;
};

#endif /* IMAGE_H */
