/**
 * @file bmp.c
 * @brief bmptool, a host tool for RetrOS's images.
 *
 * The format: an 8-bit, uncompressed BMP whose color table is the standard VGA palette (lib/palette.c), except entry
 * 255, the transparent color (COLOR_TRANSPARENT), which is magenta so an image editor shows it. Each pixel is a
 * palette index.
 *
 *   bmptool raw2bmp WIDTH HEIGHT IN.raw OUT.bmp   palette indexes, row by row from the top left, to a BMP
 *   bmptool bmp2raw IN.bmp OUT.raw                a BMP's palette indexes, the same way; prints its size
 *   bmptool bmp2c OUT.c OUT.h IN.bmp...           C arrays for images built into the kernel, named after the files
 *   bmptool check IN.bmp...                       whether each file is in the format
 *
 * It's built with the host's compiler, like bin/build, so it reads palette.h after the system's headers: the kernel's
 * own stdint.h mustn't replace the system's.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <palette.h>

#define BMP_FILE_HEADER 14
#define BMP_INFO_HEADER 40
#define BMP_COLORS 256
#define BMP_PIXELS_OFFSET (BMP_FILE_HEADER + BMP_INFO_HEADER + BMP_COLORS * 4)

/* The largest width or height an image may have */
#define IMAGE_MAX_SIDE 256

#define TRANSPARENT 255

/* An image: width x height palette indexes, top row first */
struct raw_image {
    int width;
    int height;
    uint8_t* pixels;
};

static void put16(uint8_t* p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = v >> 8;
}

static void put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        p[i] = (v >> (8 * i)) & 0xff;
    }
}

static uint16_t get16(const uint8_t* p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t get32(const uint8_t* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Bytes per row in a BMP, which pads each row to a multiple of 4 */
static int bmp_stride(int width)
{
    return (width + 3) & ~3;
}

/* The color the format's table gives an index, as red, green and blue from 0 to 255 */
static void table_color(int index, uint8_t rgb[3])
{
    if (index == TRANSPARENT) {
        rgb[0] = 255;
        rgb[1] = 0;
        rgb[2] = 255;
        return;
    }
    for (int c = 0; c < 3; c++) {
        rgb[c] = PALETTE_TO_8BIT(vga_palette[index][c]);
    }
}

/* A whole file, malloc'd. Read in pieces until its end, so a pipe works too */
static uint8_t* read_file(const char* path, long* size)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "%s: can't open\n", path);
        return NULL;
    }

    long capacity = 4096;
    uint8_t* data = malloc(capacity);
    *size = 0;
    while (data != NULL) {
        if (*size == capacity) {
            capacity *= 2;
            uint8_t* bigger = realloc(data, capacity);
            if (bigger == NULL) {
                free(data);
                data = NULL;
                break;
            }
            data = bigger;
        }

        size_t got = fread(data + *size, 1, capacity - *size, f);
        *size += got;
        if (got == 0) {
            break;
        }
    }

    if (data == NULL || ferror(f)) {
        fprintf(stderr, "%s: can't read\n", path);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return data;
}

static int write_file(const char* path, const void* data, long size)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL || (long)fwrite(data, 1, size, f) != size) {
        fprintf(stderr, "%s: can't write\n", path);
        if (f != NULL) {
            fclose(f);
        }
        return -1;
    }
    fclose(f);
    return 0;
}

/* The BMP for an image, malloc'd, in the format */
static uint8_t* bmp_encode(const struct raw_image* image, long* size)
{
    int stride = bmp_stride(image->width);
    *size = BMP_PIXELS_OFFSET + (long)stride * image->height;

    uint8_t* bmp = calloc(*size, 1);
    if (bmp == NULL) {
        return NULL;
    }

    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(bmp + 2, *size);
    put32(bmp + 10, BMP_PIXELS_OFFSET);

    uint8_t* info = bmp + BMP_FILE_HEADER;
    put32(info + 0, BMP_INFO_HEADER);
    put32(info + 4, image->width);
    /* A positive height: rows are stored bottom-up, as most programs expect */
    put32(info + 8, image->height);
    put16(info + 12, 1);
    put16(info + 14, 8);
    put32(info + 16, 0);
    put32(info + 20, stride * image->height);
    put32(info + 24, 2835);
    put32(info + 28, 2835);
    put32(info + 32, BMP_COLORS);
    put32(info + 36, 0);

    /* Each color is stored blue, green, red, then a 0 */
    uint8_t* table = info + BMP_INFO_HEADER;
    for (int i = 0; i < BMP_COLORS; i++) {
        uint8_t rgb[3];
        table_color(i, rgb);
        table[4 * i + 0] = rgb[2];
        table[4 * i + 1] = rgb[1];
        table[4 * i + 2] = rgb[0];
    }

    for (int y = 0; y < image->height; y++) {
        uint8_t* row = bmp + BMP_PIXELS_OFFSET + (long)(image->height - 1 - y) * stride;
        memcpy(row, image->pixels + (long)y * image->width, image->width);
    }
    return bmp;
}

/**
 * Reads a BMP in the format, into a malloc'd image. Anything else is refused, with the reason on stderr.
 * @return 0, or -1
 */
static int bmp_decode(const char* path, const uint8_t* data, long size, struct raw_image* image)
{
    if (size < BMP_FILE_HEADER + BMP_INFO_HEADER || data[0] != 'B' || data[1] != 'M') {
        fprintf(stderr, "%s: not a BMP\n", path);
        return -1;
    }

    const uint8_t* info = data + BMP_FILE_HEADER;
    uint32_t header = get32(info + 0);
    int32_t width = (int32_t)get32(info + 4);
    int32_t height = (int32_t)get32(info + 8);
    uint32_t offset = get32(data + 10);
    uint32_t colors = get32(info + 32);

    if (header < BMP_INFO_HEADER || header > (uint32_t)size) {
        fprintf(stderr, "%s: unknown header size %u\n", path, header);
        return -1;
    }
    if (get16(info + 12) != 1 || get16(info + 14) != 8) {
        fprintf(stderr, "%s: %d bits a pixel; only 8 are supported\n", path, get16(info + 14));
        return -1;
    }
    if (get32(info + 16) != 0) {
        fprintf(stderr, "%s: compressed; only uncompressed BMPs are supported\n", path);
        return -1;
    }

    /* A negative height means the rows are stored top-down */
    int top_down = height < 0;
    if (top_down) {
        height = -height;
    }
    if (width < 1 || width > IMAGE_MAX_SIDE || height < 1 || height > IMAGE_MAX_SIDE) {
        fprintf(stderr, "%s: %dx%d; each side must be 1 to %d\n", path, width, height, IMAGE_MAX_SIDE);
        return -1;
    }

    if (colors == 0) {
        colors = BMP_COLORS;
    }
    long table = BMP_FILE_HEADER + (long)header;
    if (colors != BMP_COLORS || table + BMP_COLORS * 4 > size) {
        fprintf(stderr, "%s: its color table isn't the 256 colors of the VGA palette\n", path);
        return -1;
    }
    for (int i = 0; i < BMP_COLORS; i++) {
        uint8_t rgb[3];
        table_color(i, rgb);
        const uint8_t* entry = data + table + 4 * i;
        if (entry[0] != rgb[2] || entry[1] != rgb[1] || entry[2] != rgb[0]) {
            fprintf(stderr, "%s: color %d is (%d, %d, %d), not the VGA palette's (%d, %d, %d)\n", path, i,
                entry[2], entry[1], entry[0], rgb[0], rgb[1], rgb[2]);
            return -1;
        }
    }

    int stride = bmp_stride(width);
    if (offset < table + BMP_COLORS * 4 || offset > (uint32_t)size || (long)stride * height > size - (long)offset) {
        fprintf(stderr, "%s: its pixels don't fit in the file\n", path);
        return -1;
    }

    image->width = width;
    image->height = height;
    image->pixels = malloc((long)width * height);
    if (image->pixels == NULL) {
        fprintf(stderr, "%s: out of memory\n", path);
        return -1;
    }
    for (int y = 0; y < height; y++) {
        int stored = top_down ? y : height - 1 - y;
        memcpy(image->pixels + (long)y * width, data + offset + (long)stored * stride, width);
    }
    return 0;
}

static int load(const char* path, struct raw_image* image)
{
    long size;
    uint8_t* data = read_file(path, &size);
    if (data == NULL) {
        return -1;
    }
    int ret = bmp_decode(path, data, size, image);
    free(data);
    return ret;
}

/* The C name for an image file: its name without the directory or extension, which has to be an identifier */
static int image_name(const char* path, char* name, int size)
{
    const char* base = strrchr(path, '/');
    base = base != NULL ? base + 1 : path;
    const char* dot = strrchr(base, '.');
    int len = dot != NULL ? (int)(dot - base) : (int)strlen(base);

    if (len < 1 || len >= size || !(isalpha((unsigned char)base[0]) || base[0] == '_')) {
        fprintf(stderr, "%s: its name isn't a C identifier\n", path);
        return -1;
    }
    for (int i = 0; i < len; i++) {
        if (!(isalnum((unsigned char)base[i]) || base[i] == '_')) {
            fprintf(stderr, "%s: its name isn't a C identifier\n", path);
            return -1;
        }
        name[i] = base[i];
    }
    name[len] = 0;
    return 0;
}

static int cmd_raw2bmp(int argc, char* argv[])
{
    if (argc != 6) {
        fprintf(stderr, "usage: bmptool raw2bmp WIDTH HEIGHT IN.raw OUT.bmp\n");
        return 1;
    }

    struct raw_image image = { atoi(argv[2]), atoi(argv[3]), NULL };
    if (image.width < 1 || image.width > IMAGE_MAX_SIDE || image.height < 1 || image.height > IMAGE_MAX_SIDE) {
        fprintf(stderr, "each side must be 1 to %d\n", IMAGE_MAX_SIDE);
        return 1;
    }

    long size;
    image.pixels = read_file(argv[4], &size);
    if (image.pixels == NULL) {
        return 1;
    }
    if (size != (long)image.width * image.height) {
        fprintf(stderr, "%s: %ld bytes, but %dx%d is %d\n", argv[4], size, image.width, image.height,
            image.width * image.height);
        return 1;
    }

    long bmp_size;
    uint8_t* bmp = bmp_encode(&image, &bmp_size);
    return bmp == NULL || write_file(argv[5], bmp, bmp_size) < 0;
}

static int cmd_bmp2raw(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: bmptool bmp2raw IN.bmp OUT.raw\n");
        return 1;
    }

    struct raw_image image;
    if (load(argv[2], &image) < 0 || write_file(argv[3], image.pixels, (long)image.width * image.height) < 0) {
        return 1;
    }
    printf("%d %d\n", image.width, image.height);
    return 0;
}

static int cmd_bmp2c(int argc, char* argv[])
{
    if (argc < 5) {
        fprintf(stderr, "usage: bmptool bmp2c OUT.c OUT.h IN.bmp...\n");
        return 1;
    }

    int count = argc - 4;
    struct raw_image* images = calloc(count, sizeof(struct raw_image));
    char (*names)[64] = calloc(count, 64);
    if (images == NULL || names == NULL) {
        return 1;
    }

    /* All of them are read before anything is written, so a bad image leaves no half-written output */
    for (int i = 0; i < count; i++) {
        if (image_name(argv[4 + i], names[i], 64) < 0 || load(argv[4 + i], &images[i]) < 0) {
            return 1;
        }
    }

    const char* header = strrchr(argv[3], '/');
    header = header != NULL ? header + 1 : argv[3];

    FILE* c = fopen(argv[2], "w");
    FILE* h = fopen(argv[3], "w");
    if (c == NULL || h == NULL) {
        fprintf(stderr, "can't write %s or %s\n", argv[2], argv[3]);
        return 1;
    }

    const char* note = "Generated by tools/bmp.c (bmptool bmp2c). Don't edit: change the images, and rebuild";
    fprintf(h, "/* %s */\n\n#ifndef GENERATED_IMAGES_H\n#define GENERATED_IMAGES_H\n\n#include <image.h>\n", note);
    fprintf(c, "/* %s */\n\n#include \"%s\"\n", note, header);

    for (int i = 0; i < count; i++) {
        char upper[64];
        for (int k = 0; k < 64; k++) {
            upper[k] = toupper((unsigned char)names[i][k]);
        }

        fprintf(h, "\n/* %s */\n", argv[4 + i]);
        fprintf(h, "#define IMAGE_%s_WIDTH %d\n", upper, images[i].width);
        fprintf(h, "#define IMAGE_%s_HEIGHT %d\n", upper, images[i].height);
        fprintf(h, "extern const struct image %s;\n", names[i]);

        fprintf(c, "\nstatic const unsigned char %s_pixels[%d * %d] = {\n", names[i], images[i].width, images[i].height);
        for (int y = 0; y < images[i].height; y++) {
            fprintf(c, "    ");
            for (int x = 0; x < images[i].width; x++) {
                fprintf(c, "0x%02x,", images[i].pixels[y * images[i].width + x]);
            }
            fprintf(c, "\n");
        }
        fprintf(c, "};\n");
        fprintf(c, "const struct image %s = { %d, %d, %s_pixels };\n", names[i], images[i].width, images[i].height,
            names[i]);
    }

    fprintf(h, "\n#endif /* GENERATED_IMAGES_H */\n");
    fclose(c);
    fclose(h);
    return 0;
}

static int cmd_check(int argc, char* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage: bmptool check IN.bmp...\n");
        return 1;
    }

    int failed = 0;
    for (int i = 2; i < argc; i++) {
        struct raw_image image;
        if (load(argv[i], &image) < 0) {
            failed = 1;
            continue;
        }
        printf("%s: %dx%d\n", argv[i], image.width, image.height);
        free(image.pixels);
    }
    return failed;
}

int main(int argc, char* argv[])
{
    if (argc >= 2 && strcmp(argv[1], "raw2bmp") == 0) {
        return cmd_raw2bmp(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "bmp2raw") == 0) {
        return cmd_bmp2raw(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "bmp2c") == 0) {
        return cmd_bmp2c(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "check") == 0) {
        return cmd_check(argc, argv);
    }

    fprintf(stderr,
        "usage: bmptool raw2bmp WIDTH HEIGHT IN.raw OUT.bmp\n"
        "       bmptool bmp2raw IN.bmp OUT.raw\n"
        "       bmptool bmp2c OUT.c OUT.h IN.bmp...\n"
        "       bmptool check IN.bmp...\n");
    return 1;
}
