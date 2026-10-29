/**
 * Host tests for lib/bmp.c, the image reader the kernel runs on files from the disk. run.sh builds it with
 * AddressSanitizer, so a read outside a file's data fails the run. Files given as arguments must parse with the
 * palette's own colors: run.sh passes every BMP in images/ and rootfs/sysutil/icons/.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bmp.h>
#include <palette.h>

static int failures = 0;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* What a test file is made of */
struct spec {
    int width;
    int height;          /* Negative for top-down rows */
    int header_size;     /* 40, or a later header's 108 or 124 */
    int colors;          /* Entries in the table; the header says 0 when it's 256 and zero_means_256 */
    int zero_means_256;
    int bpp;
    int compression;
    int planes;
    const uint8_t (*table)[3]; /* Red, green and blue from 0 to 255 for each entry */
};

static void put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        p[i] = (v >> (8 * i)) & 0xff;
    }
}

/* A BMP of spec's layout, with pixels (indexes into its own table, top row first), malloc'd */
static uint8_t* make_bmp(const struct spec* s, const uint8_t* pixels, int* size)
{
    int height = s->height < 0 ? -s->height : s->height;
    int stride = (s->width + 3) & ~3;
    int offset = 14 + s->header_size + 4 * s->colors;
    *size = offset + stride * height;

    uint8_t* b = calloc(*size, 1);
    b[0] = 'B';
    b[1] = 'M';
    put32(b + 2, *size);
    put32(b + 10, offset);
    put32(b + 14, s->header_size);
    put32(b + 18, s->width);
    put32(b + 22, s->height);
    b[26] = s->planes;
    b[28] = s->bpp;
    put32(b + 30, s->compression);
    put32(b + 46, s->colors == 256 && s->zero_means_256 ? 0 : s->colors);

    uint8_t* table = b + 14 + s->header_size;
    for (int i = 0; i < s->colors; i++) {
        table[4 * i + 0] = s->table[i][2];
        table[4 * i + 1] = s->table[i][1];
        table[4 * i + 2] = s->table[i][0];
    }
    for (int y = 0; y < height; y++) {
        int stored = s->height < 0 ? y : height - 1 - y;
        memcpy(b + offset + stored * stride, pixels + y * s->width, s->width);
    }
    return b;
}

static uint8_t vga[256][3];

/* The table bmptool writes */
static void make_vga_table(void)
{
    for (int i = 0; i < 256; i++) {
        palette_file_color(i, vga[i]);
    }
}

static struct spec vga_spec(int width, int height)
{
    struct spec s = { width, height, 40, 256, 1, 8, 0, 1, (const uint8_t (*)[3])vga };
    return s;
}

static uint32_t seed = 12345;

static uint32_t next_random(void)
{
    seed = seed * 1103515245 + 12345;
    return seed >> 8;
}

/* Parses and decodes a file, checking the result is pixels. @return whether it parsed */
static int expect_image(const uint8_t* bmp, int size, int width, int height, const uint8_t* pixels, int remapped,
    const char* what)
{
    struct bmp_info info;
    if (bmp_parse(bmp, size, &info) < 0) {
        CHECK(0, "%s: refused", what);
        return 0;
    }
    CHECK(info.width == width && info.height == height, "%s: %dx%d, not %dx%d", what, info.width, info.height, width,
        height);
    CHECK(info.remapped == remapped, "%s: remapped is %d", what, info.remapped);

    uint8_t* out = malloc(width * height);
    bmp_decode(bmp, &info, out);
    CHECK(memcmp(out, pixels, width * height) == 0, "%s: pixels differ", what);
    free(out);
    return 1;
}

static void expect_refused(const uint8_t* bmp, int size, const char* what)
{
    struct bmp_info info;
    CHECK(bmp_parse(bmp, size, &info) < 0, "%s: accepted", what);
}

static uint8_t* random_pixels(int count, int (*allowed)(int))
{
    uint8_t* p = malloc(count);
    for (int i = 0; i < count; i++) {
        int v;
        do {
            v = next_random() & 0xff;
        } while (allowed != NULL && !allowed(v));
        p[i] = v;
    }
    return p;
}

/* Indexes whose color no lower index has, so remapping by color gives them back */
static int distinct_color(int i)
{
    return i == 255 || (i != 16 && i != 31 && i < 248);
}

static void test_layouts(void)
{
    const int sizes[][2] = { { 1, 1 }, { 3, 5 }, { 85, 95 }, { 256, 256 }, { 255, 1 }, { 1, 256 } };
    for (unsigned int k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
        int w = sizes[k][0], h = sizes[k][1];
        uint8_t* pixels = random_pixels(w * h, NULL);
        char what[64];
        int size;

        struct spec s = vga_spec(w, h);
        uint8_t* bmp = make_bmp(&s, pixels, &size);
        snprintf(what, sizeof(what), "%dx%d bottom-up", w, h);
        expect_image(bmp, size, w, h, pixels, 0, what);
        free(bmp);

        s.height = -h;
        bmp = make_bmp(&s, pixels, &size);
        snprintf(what, sizeof(what), "%dx%d top-down", w, h);
        expect_image(bmp, size, w, h, pixels, 0, what);
        free(bmp);

        s = vga_spec(w, h);
        s.header_size = 124;
        s.zero_means_256 = 0;
        bmp = make_bmp(&s, pixels, &size);
        snprintf(what, sizeof(what), "%dx%d with a 124-byte header", w, h);
        expect_image(bmp, size, w, h, pixels, 0, what);
        free(bmp);
        free(pixels);
    }
}

/* An image editor's file: the same colors, in another order, with magenta for transparent somewhere else */
static void test_reordered_palette(void)
{
    int w = 40, h = 30;
    uint8_t* pixels = random_pixels(w * h, distinct_color);

    int order[256];
    for (int i = 0; i < 256; i++) {
        order[i] = i;
    }
    for (int i = 255; i > 0; i--) {
        int j = next_random() % (i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }

    /* Entry i of the file's table is palette color order[i], so palette index p is stored as where[p] */
    static uint8_t table[256][3];
    int where[256];
    for (int i = 0; i < 256; i++) {
        memcpy(table[i], vga[order[i]], 3);
        where[order[i]] = i;
    }
    uint8_t* stored = malloc(w * h);
    for (int i = 0; i < w * h; i++) {
        stored[i] = where[pixels[i]];
    }

    struct spec s = vga_spec(w, h);
    s.table = (const uint8_t (*)[3])table;
    int size;
    uint8_t* bmp = make_bmp(&s, stored, &size);
    expect_image(bmp, size, w, h, pixels, 1, "a reordered palette");
    free(bmp);
    free(stored);
    free(pixels);
}

/* A table of a few colors, some not the palette's: each goes to the nearest, and an index past the table to nothing */
static void test_few_colors(void)
{
    static const uint8_t table[4][3] = { { 0, 0, 0 }, { 255, 0, 255 }, { 250, 10, 10 }, { 200, 200, 200 } };
    const uint8_t stored[6] = { 0, 1, 2, 3, 7, 0 };

    int best[4];
    for (int t = 0; t < 4; t++) {
        long best_distance = -1;
        for (int i = 0; i < 255; i++) {
            long d = 0;
            for (int c = 0; c < 3; c++) {
                long diff = (long)table[t][c] - vga[i][c];
                d += diff * diff;
            }
            if (best_distance < 0 || d < best_distance) {
                best_distance = d;
                best[t] = i;
            }
        }
    }
    const uint8_t expected[6] = { 0, 255, best[2], best[3], 255, 0 };
    CHECK(best[0] == 0, "black isn't 0");
    CHECK(vga_palette[best[2]][0] > 50 && vga_palette[best[2]][1] < 10 && vga_palette[best[2]][2] < 10,
        "(250, 10, 10) became %d, not a red", best[2]);

    struct spec s = vga_spec(3, 2);
    s.colors = 4;
    s.table = table;
    int size;
    uint8_t* bmp = make_bmp(&s, stored, &size);
    expect_image(bmp, size, 3, 2, expected, 1, "four colors");
    free(bmp);
}

static void test_refused(void)
{
    uint8_t* pixels = random_pixels(85 * 95, NULL);
    int size;
    struct spec good = vga_spec(85, 95);
    uint8_t* bmp = make_bmp(&good, pixels, &size);

    /* Every prefix: each one cuts off part of the pixels, or more */
    for (int n = 0; n < size; n++) {
        uint8_t* cut = malloc(n > 0 ? n : 1);
        memcpy(cut, bmp, n);
        struct bmp_info info;
        if (bmp_parse(cut, n, &info) == 0) {
            CHECK(0, "the first %d of %d bytes accepted", n, size);
        }
        free(cut);
    }
    expect_refused(NULL, size, "no data");
    expect_refused(bmp, -1, "a negative size");

    struct {
        const char* what;
        int at;
        uint32_t value;
        int bytes;
    } changes[] = {
        { "not BM", 0, 'X', 1 },
        { "a 12-byte header", 14, 12, 4 },
        { "a header past the end", 14, 0x7fffffff, 4 },
        { "a width of 0", 18, 0, 4 },
        { "a negative width", 18, (uint32_t)-1, 4 },
        { "a width of 257", 18, 257, 4 },
        { "a height of 0", 22, 0, 4 },
        { "a height of 257", 22, 257, 4 },
        { "a height of -257", 22, (uint32_t)-257, 4 },
        { "the most negative height", 22, 0x80000000, 4 },
        { "2 planes", 26, 2, 2 },
        { "4 bits a pixel", 28, 4, 2 },
        { "24 bits a pixel", 28, 24, 2 },
        { "32 bits a pixel", 28, 32, 2 },
        { "RLE compression", 30, 1, 4 },
        { "257 colors", 46, 257, 4 },
        { "a huge color count", 46, 0xffffffff, 4 },
        { "pixels inside the color table", 10, 54 + 100, 4 },
        { "pixels past the end", 10, 0x7fffff00, 4 },
        { "pixels starting one byte late", 10, 1079, 4 },
    };
    for (unsigned int k = 0; k < sizeof(changes) / sizeof(changes[0]); k++) {
        uint8_t* bad = malloc(size);
        memcpy(bad, bmp, size);
        for (int i = 0; i < changes[k].bytes; i++) {
            bad[changes[k].at + i] = (changes[k].value >> (8 * i)) & 0xff;
        }
        expect_refused(bad, size, changes[k].what);
        free(bad);
    }

    free(bmp);
    free(pixels);
}

/* Random damage: whatever bmp_parse accepts, bmp_decode must decode without reading outside the file */
static void test_damage(void)
{
    uint8_t* pixels = random_pixels(20 * 12, NULL);
    int size;
    struct spec good = vga_spec(20, 12);
    uint8_t* bmp = make_bmp(&good, pixels, &size);

    int accepted = 0;
    for (int round = 0; round < 50000; round++) {
        int n = size;
        if (next_random() % 4 == 0) {
            n = next_random() % (size + 1);
        }
        uint8_t* bad = malloc(n > 0 ? n : 1);
        memcpy(bad, bmp, n);
        int flips = 1 + next_random() % 8;
        for (int f = 0; f < flips && n > 0; f++) {
            /* Mostly the headers, where the sizes and offsets are */
            int at = next_random() % 2 ? next_random() % (n < 60 ? n : 60) : next_random() % n;
            bad[at] = next_random() & 0xff;
        }

        struct bmp_info info;
        if (bmp_parse(bad, n, &info) == 0) {
            accepted++;
            CHECK(info.width >= 1 && info.width <= BMP_MAX_SIDE && info.height >= 1 && info.height <= BMP_MAX_SIDE,
                "accepted %dx%d", info.width, info.height);
            uint8_t* out = malloc(info.width * info.height);
            bmp_decode(bad, &info, out);
            free(out);
        }
        free(bad);
    }
    printf("     damage: %d of 50000 damaged files still parsed, and decoded in bounds\n", accepted);
    free(bmp);
    free(pixels);
}

static void test_files(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "rb");
        static uint8_t data[1 << 20];
        int size = f != NULL ? (int)fread(data, 1, sizeof(data), f) : -1;
        if (f != NULL) {
            fclose(f);
        }

        struct bmp_info info;
        if (size < 0 || bmp_parse(data, size, &info) < 0) {
            CHECK(0, "%s: refused", argv[i]);
            continue;
        }
        CHECK(!info.remapped, "%s: its colors aren't the palette's", argv[i]);
    }
    printf("     files: %d checked\n", argc - 1);
}

int main(int argc, char* argv[])
{
    make_vga_table();
    test_layouts();
    test_reordered_palette();
    test_few_colors();
    test_refused();
    test_damage();
    test_files(argc, argv);

    printf("%s lib/bmp.c\n", failures == 0 ? "ok  " : "FAIL");
    return failures != 0;
}
