#ifndef IMAGE_H
#define IMAGE_H

/**
 * An image in memory: width x height palette indexes, row by row from the top left, with COLOR_TRANSPARENT (255) where
 * nothing is drawn. Images built into the kernel are generated from the BMPs in images/ (tools/bmp.c, bmp2c)
 */
struct image {
    int width;
    int height;
    const unsigned char* pixels;
};

/* Where the images loaded at run time are, each NAME.bmp, shared by the kernel (icon_get) and apps (gfx_load_icon) */
#define ICONS_DIR "/sysutil/icons/"

#endif /* IMAGE_H */
