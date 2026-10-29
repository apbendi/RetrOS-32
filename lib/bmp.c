/**
 * @file bmp.c
 * @brief Reading image files: 8-bit, uncompressed BMPs whose colors are the VGA palette's (include/bmp.h).
 *
 * The kernel runs this on files from the disk, so every offset and size in a file is checked against the data before
 * it's used, and nothing outside the data is read.
 */

#define ERRORS_CODES_ONLY
#include <errors.h>
#include <bmp.h>
#include <palette.h>

#define BMP_FILE_HEADER 14
#define BMP_INFO_HEADER 40

#define BMP_TRANSPARENT PALETTE_TRANSPARENT

static unsigned int get16(const unsigned char* p)
{
    return p[0] | (p[1] << 8);
}

static unsigned int get32(const unsigned char* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24);
}

int bmp_parse(const unsigned char* data, int size, struct bmp_info* info)
{
    if (data == 0 || info == 0 || size < BMP_FILE_HEADER + BMP_INFO_HEADER || data[0] != 'B' || data[1] != 'M') {
        return -ERROR_BAD_IMAGE;
    }

    const unsigned char* header = data + BMP_FILE_HEADER;
    unsigned int header_size = get32(header);
    /* Later versions of the header start with the same 40 bytes. An older one, 12 bytes, isn't read */
    if (header_size < BMP_INFO_HEADER || header_size > (unsigned int)size - BMP_FILE_HEADER) {
        return -ERROR_BAD_IMAGE;
    }

    int width = (int)get32(header + 4);
    int height = (int)get32(header + 8);
    if (get16(header + 12) != 1 || get16(header + 14) != 8 || get32(header + 16) != 0) {
        return -ERROR_BAD_IMAGE;
    }

    /* A negative height means the rows are stored top-down. Its range is checked before it's negated */
    if (width < 1 || width > BMP_MAX_SIDE || height < -BMP_MAX_SIDE || height > BMP_MAX_SIDE || height == 0) {
        return -ERROR_BAD_IMAGE;
    }
    int top_down = height < 0;
    if (top_down) {
        height = -height;
    }

    unsigned int colors = get32(header + 32);
    if (colors == 0) {
        colors = 256;
    }
    unsigned int table = BMP_FILE_HEADER + header_size;
    if (colors > 256 || colors * 4 > (unsigned int)size - table) {
        return -ERROR_BAD_IMAGE;
    }

    /* The rows: after the color table, each padded to a multiple of 4 bytes, all inside the file */
    int stride = (width + 3) & ~3;
    unsigned int offset = get32(data + 10);
    if (offset < table + colors * 4 || offset > (unsigned int)size ||
        (unsigned int)(stride * height) > (unsigned int)size - offset) {
        return -ERROR_BAD_IMAGE;
    }

    /* Each color is stored blue, green, red, then a byte that isn't used */
    const unsigned char* entries = data + table;
    int matches = colors >= BMP_TRANSPARENT;
    for (int i = 0; i < BMP_TRANSPARENT && matches; i++) {
        const unsigned char* entry = entries + 4 * i;
        uint8_t rgb[3];
        palette_file_color(i, rgb);
        matches = entry[2] == rgb[0] && entry[1] == rgb[1] && entry[0] == rgb[2];
    }

    for (int i = 0; i < 256; i++) {
        if (matches) {
            info->colors[i] = i;
        } else if ((unsigned int)i >= colors) {
            /* An index the table doesn't have is drawn as nothing */
            info->colors[i] = BMP_TRANSPARENT;
        } else {
            const unsigned char* entry = entries + 4 * i;
            int magenta = entry[2] == 255 && entry[1] == 0 && entry[0] == 255;
            info->colors[i] = magenta ? BMP_TRANSPARENT : palette_nearest(entry[2], entry[1], entry[0]);
        }
    }

    info->width = width;
    info->height = height;
    info->remapped = !matches;
    info->offset = offset;
    info->stride = stride;
    info->top_down = top_down;
    return 0;
}

void bmp_decode(const unsigned char* data, const struct bmp_info* info, unsigned char* pixels)
{
    for (int y = 0; y < info->height; y++) {
        int stored = info->top_down ? y : info->height - 1 - y;
        const unsigned char* row = data + info->offset + stored * info->stride;
        for (int x = 0; x < info->width; x++) {
            pixels[y * info->width + x] = info->colors[row[x]];
        }
    }
}
