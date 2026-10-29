/**
 * @file bmp.c
 * @brief bmptool, a host tool for RetrOS's images.
 *
 * The format: an 8-bit, uncompressed BMP whose color table is the standard VGA palette (lib/palette.c), except entry
 * 255, the transparent color (COLOR_TRANSPARENT), which is magenta so an image editor shows it, and the palette's own
 * magenta, listed one step darker so it isn't taken for transparent (palette_file_color). Each pixel is a palette
 * index.
 *
 *   bmptool raw2bmp WIDTH HEIGHT IN.raw OUT.bmp   palette indexes, row by row from the top left, to a BMP
 *   bmptool bmp2raw IN.bmp OUT.raw                a BMP's palette indexes, the same way; prints its size
 *   bmptool bmp2c OUT.c OUT.h IN.bmp...           C arrays for images built into the kernel, named after the files
 *   bmptool check IN.bmp...                       whether RetrOS can read each file, and whether its colors are
 *                                                 the palette's or have to be mapped to it
 *   bmptool import IN.bmp OUT.bmp                 any image editor's BMP, 24- or 32-bit or 8-bit, in the format:
 *                                                 each pixel the palette's nearest color, and transparent where
 *                                                 its alpha is below half or it's magenta
 *
 * Files are read with lib/bmp.c, as the kernel and apps read them.
 * It's built with the host's compiler, like bin/build, so it reads palette.h after the system's headers: the kernel's
 * own stdint.h mustn't replace the system's.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bmp.h>
#include <palette.h>

#define BMP_FILE_HEADER 14
#define BMP_INFO_HEADER 40
#define BMP_COLORS 256
#define BMP_PIXELS_OFFSET (BMP_FILE_HEADER + BMP_INFO_HEADER + BMP_COLORS * 4)

/* The largest width or height an image may have */
#define IMAGE_MAX_SIDE BMP_MAX_SIDE


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
        palette_file_color(i, rgb);
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

/* Why a file bmp_parse refused isn't one RetrOS reads, from the fields most often wrong */
static void explain(const char* path, const uint8_t* data, long size)
{
    if (size < BMP_FILE_HEADER + BMP_INFO_HEADER || data[0] != 'B' || data[1] != 'M') {
        fprintf(stderr, "%s: not a BMP\n", path);
        return;
    }
    const uint8_t* info = data + BMP_FILE_HEADER;
    int width = (int32_t)get32(info + 4);
    int height = (int32_t)get32(info + 8);
    if (get16(info + 14) != 8) {
        fprintf(stderr, "%s: %d bits a pixel; only 8 are read, and bmptool import converts 24 and 32\n", path,
            get16(info + 14));
    } else if (get32(info + 16) != 0) {
        fprintf(stderr, "%s: compressed; only uncompressed BMPs are read\n", path);
    } else if (width < 1 || width > IMAGE_MAX_SIDE || height == 0 || height < -IMAGE_MAX_SIDE ||
        height > IMAGE_MAX_SIDE) {
        fprintf(stderr, "%s: %dx%d; each side must be 1 to %d\n", path, width, height < 0 ? -height : height,
            IMAGE_MAX_SIDE);
    } else {
        fprintf(stderr, "%s: its headers, color table or pixels don't fit in the file\n", path);
    }
}

/**
 * Reads an image file with lib/bmp.c, the kernel's and apps' reader, into a malloc'd image. remapped says whether its
 * colors weren't the palette's.
 * @return 0, or -1, with the reason on stderr
 */
static int bmp_read(const char* path, const uint8_t* data, long size, struct raw_image* image, int* remapped)
{
    struct bmp_info info;
    if (size > 0x7fffffff || bmp_parse(data, (int)size, &info) < 0) {
        explain(path, data, size);
        return -1;
    }

    image->width = info.width;
    image->height = info.height;
    image->pixels = malloc((long)info.width * info.height);
    if (image->pixels == NULL) {
        fprintf(stderr, "%s: out of memory\n", path);
        return -1;
    }
    bmp_decode(data, &info, image->pixels);
    if (remapped != NULL) {
        *remapped = info.remapped;
    }
    return 0;
}

static int load(const char* path, struct raw_image* image, int* remapped)
{
    long size;
    uint8_t* data = read_file(path, &size);
    if (data == NULL) {
        return -1;
    }
    int ret = bmp_read(path, data, size, image, remapped);
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
    if (load(argv[2], &image, NULL) < 0 || write_file(argv[3], image.pixels, (long)image.width * image.height) < 0) {
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
        if (image_name(argv[4 + i], names[i], 64) < 0 || load(argv[4 + i], &images[i], NULL) < 0) {
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

        fprintf(c, "\nstatic const unsigned char %s_pixels[%d * %d] = {\n", names[i], images[i].width,
            images[i].height);
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

/* Where a mask's bits are, and how many there are, to read a channel out of a pixel and scale it to 0 to 255 */
struct channel {
    uint32_t mask;
    int shift;
    int bits;
};

static struct channel channel_of(uint32_t mask)
{
    struct channel c = { mask, 0, 0 };
    while (mask != 0 && !(mask & 1)) {
        mask >>= 1;
        c.shift++;
    }
    while (mask & 1) {
        mask >>= 1;
        c.bits++;
    }
    return c;
}

static int channel_value(const struct channel* c, uint32_t pixel)
{
    if (c->bits == 0) {
        return 0;
    }
    uint32_t v = (pixel & c->mask) >> c->shift;
    uint32_t max = c->bits >= 32 ? 0xffffffff : (1u << c->bits) - 1;
    return (int)((v * 255 + max / 2) / max);
}

/**
 * Reads a 24- or 32-bit BMP, the kind any image editor exports, into an image in the palette's colors. A 32-bit file's
 * alpha counts if its header gives an alpha mask, or, without masks, if any pixel's alpha byte isn't 0, since many
 * programs leave that byte 0 everywhere.
 * @return 0, or -1, with the reason on stderr
 */
static int import_true_color(const char* path, const uint8_t* data, long size, struct raw_image* image)
{
    const uint8_t* info = data + BMP_FILE_HEADER;
    uint32_t header = get32(info);
    int32_t width = (int32_t)get32(info + 4);
    int32_t height = (int32_t)get32(info + 8);
    int bpp = get16(info + 14);
    uint32_t compression = get32(info + 16);

    if (header < BMP_INFO_HEADER || header > (uint32_t)(size - BMP_FILE_HEADER) || get16(info + 12) != 1 ||
        !(bpp == 24 || bpp == 32) || !(compression == 0 || (compression == 3 && bpp == 32))) {
        fprintf(stderr, "%s: only uncompressed 8-, 24- and 32-bit BMPs are imported\n", path);
        return -1;
    }

    int top_down = height < 0;
    if (top_down) {
        height = -height;
    }
    if (width < 1 || width > IMAGE_MAX_SIDE || height < 1 || height > IMAGE_MAX_SIDE) {
        fprintf(stderr, "%s: %dx%d; each side must be 1 to %d\n", path, width, height, IMAGE_MAX_SIDE);
        return -1;
    }

    /* Plain 24- and 32-bit pixels are blue, green, red, and for 32 bits a fourth byte */
    struct channel red = channel_of(0x00ff0000), green = channel_of(0x0000ff00), blue = channel_of(0x000000ff);
    struct channel alpha = channel_of(bpp == 32 ? 0xff000000 : 0);
    int alpha_given = 0;
    if (compression == 3) {
        /* The masks are in the header, from a version 3 header on, or follow a 40-byte one */
        long masks = BMP_FILE_HEADER + BMP_INFO_HEADER;
        if (masks + 12 > size) {
            fprintf(stderr, "%s: its color masks don't fit in the file\n", path);
            return -1;
        }
        red = channel_of(get32(data + masks));
        green = channel_of(get32(data + masks + 4));
        blue = channel_of(get32(data + masks + 8));
        alpha = channel_of(header >= 56 ? get32(info + 52) : 0);
        alpha_given = alpha.bits > 0;
    }

    int bytes = bpp / 8;
    long stride = ((long)width * bytes + 3) & ~3L;
    uint32_t offset = get32(data + 10);
    if (offset < BMP_FILE_HEADER + header || offset > (uint32_t)size || stride * height > size - (long)offset) {
        fprintf(stderr, "%s: its pixels don't fit in the file\n", path);
        return -1;
    }

    int use_alpha = alpha_given;
    for (int y = 0; y < height && bpp == 32 && compression == 0 && !use_alpha; y++) {
        for (int x = 0; x < width && !use_alpha; x++) {
            use_alpha = data[offset + y * stride + x * 4 + 3] != 0;
        }
    }

    image->width = width;
    image->height = height;
    image->pixels = malloc((long)width * height);
    if (image->pixels == NULL) {
        fprintf(stderr, "%s: out of memory\n", path);
        return -1;
    }

    for (int y = 0; y < height; y++) {
        const uint8_t* row = data + offset + (top_down ? y : height - 1 - y) * stride;
        for (int x = 0; x < width; x++) {
            const uint8_t* p = row + x * bytes;
            uint32_t pixel = p[0] | (p[1] << 8) | (p[2] << 16) | (bytes == 4 ? (uint32_t)p[3] << 24 : 0);
            int r = channel_value(&red, pixel), g = channel_value(&green, pixel), b = channel_value(&blue, pixel);
            int transparent = (use_alpha && channel_value(&alpha, pixel) < 128) || (r == 255 && g == 0 && b == 255);
            image->pixels[y * width + x] = transparent ? PALETTE_TRANSPARENT : palette_nearest(r, g, b);
        }
    }
    return 0;
}

static int cmd_import(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: bmptool import IN.bmp OUT.bmp\n");
        return 1;
    }

    long size;
    uint8_t* data = read_file(argv[2], &size);
    if (data == NULL) {
        return 1;
    }
    if (size < BMP_FILE_HEADER + BMP_INFO_HEADER || data[0] != 'B' || data[1] != 'M') {
        fprintf(stderr, "%s: not a BMP\n", argv[2]);
        return 1;
    }

    /* An 8-bit file is read as the kernel reads it, its colors mapped to the palette's */
    struct raw_image image;
    int bpp = get16(data + BMP_FILE_HEADER + 14);
    int ret = bpp == 8 ? bmp_read(argv[2], data, size, &image, NULL) : import_true_color(argv[2], data, size, &image);
    if (ret < 0) {
        return 1;
    }

    long bmp_size;
    uint8_t* bmp = bmp_encode(&image, &bmp_size);
    if (bmp == NULL || write_file(argv[3], bmp, bmp_size) < 0) {
        return 1;
    }
    printf("%s: %dx%d\n", argv[3], image.width, image.height);
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
        int remapped;
        if (load(argv[i], &image, &remapped) < 0) {
            failed = 1;
            continue;
        }
        /* A file with other colors is read, but bmptool's own files have the palette's */
        printf("%s: %dx%d%s\n", argv[i], image.width, image.height,
            remapped ? ", its colors mapped to the palette's nearest" : "");
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
    if (argc >= 2 && strcmp(argv[1], "import") == 0) {
        return cmd_import(argc, argv);
    }

    fprintf(stderr,
        "usage: bmptool raw2bmp WIDTH HEIGHT IN.raw OUT.bmp\n"
        "       bmptool bmp2raw IN.bmp OUT.raw\n"
        "       bmptool bmp2c OUT.c OUT.h IN.bmp...\n"
        "       bmptool check IN.bmp...\n"
        "       bmptool import IN.bmp OUT.bmp\n");
    return 1;
}
