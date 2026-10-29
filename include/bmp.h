#ifndef BMP_H
#define BMP_H

/**
 * Reading RetrOS's image files: 8-bit, uncompressed BMPs (#177). lib/bmp.c is built into the kernel and libgraphic, and
 * bmptool compiles it on the host. It never reads outside the data it's given, since the kernel runs it on files from
 * the disk.
 *
 * Colors: a file whose color table lists the palette as palette_file_color does (palette.h), apart from entry 255, is
 * used as it is, and its index 255 is transparent whatever color the table gives it. bmptool writes such files. A file
 * an image editor saved with other colors, or in another order, is remapped: magenta, (255, 0, 255), becomes
 * COLOR_TRANSPARENT, and every other color the palette's nearest.
 */

/* The largest width or height an image file may have */
#define BMP_MAX_SIDE 256

/* What bmp_parse found in a file, for bmp_decode */
struct bmp_info {
    int width;
    int height;
    /* The file's colors weren't the palette's, so bmp_decode maps them to it */
    int remapped;

    /* For bmp_decode: where the rows start, how far apart they are, which comes first, and each index's color */
    unsigned int offset;
    int stride;
    int top_down;
    unsigned char colors[256];
};

/**
 * Checks that data, size bytes, is an image file RetrOS can read: an 8-bit, uncompressed BMP, each side 1 to
 * BMP_MAX_SIDE. Works out how its colors map to the palette.
 * @return 0, or -ERROR_BAD_IMAGE
 */
int bmp_parse(const unsigned char* data, int size, struct bmp_info* info);

/**
 * Writes the image bmp_parse checked into pixels, which holds info->width * info->height bytes: each pixel's palette
 * index, row by row from the top left, with COLOR_TRANSPARENT where it's transparent.
 */
void bmp_decode(const unsigned char* data, const struct bmp_info* info, unsigned char* pixels);

#endif /* BMP_H */
