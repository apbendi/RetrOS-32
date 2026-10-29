#ifndef VBE_H
#define VBE_H

#include <stdint.h>
#include <colors.h>

#define PIXELS_PER_CHAR 8
#define PIXELS_PER_ICON 16

extern uint8_t test_icon32[32][32];

#define VBE_SIZE() (vbe_info->width*vbe_info->height*(vbe_info->bpp/8)+1)

extern struct vbe_mode_info_structure* vbe_info;
struct vbe_mode_info_structure {
   uint16_t attributes;      // deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
   uint8_t window_a;         // deprecated
   uint8_t window_b;         // deprecated
   uint16_t granularity;      // deprecated; used while calculating bank numbers
   uint16_t window_size;
   uint16_t segment_a;
   uint16_t segment_b;
   uint32_t win_func_ptr;      // deprecated; used to switch banks from protected mode without returning to real mode
   uint16_t pitch;         // number of bytes per horizontal line
   uint16_t width;         // width in pixels
   uint16_t height;         // height in pixels
   uint8_t w_char;         // unused...
   uint8_t y_char;         // ...
   uint8_t planes;
   uint8_t bpp;         // bits per pixel in this mode
   uint8_t banks;         // deprecated; total number of banks in this mode
   uint8_t memory_model;
   uint8_t bank_size;      // deprecated; size of a bank, almost always 64 KB but may be 16 KB...
   uint8_t image_pages;
   uint8_t reserved0;

   uint8_t red_mask;
   uint8_t red_position;
   uint8_t green_mask;
   uint8_t green_position;
   uint8_t blue_mask;
   uint8_t blue_position;
   uint8_t reserved_mask;
   uint8_t reserved_position;
   uint8_t direct_color_attributes;

   uint32_t framebuffer;      // physical address of the linear frame buffer; write here to draw to the screen
   uint32_t off_screen_mem_off;
   uint16_t off_screen_mem_size;   // size of memory in the framebuffer but not being displayed on the screen
   uint8_t reserved1[206];
} __attribute__ ((packed));

void vesa_put_char(uint8_t* buffer, unsigned char c, int x, int y, int color);
void vesa_put_box(uint8_t* buffer, unsigned char c, int x, int y, int color);
void vesa_put_block(uint8_t* buffer, unsigned char c, int x, int y, int color);
void vesa_put_char16(uint8_t* buffer, unsigned char c, int x, int y, int color);
void vesa_put_icon(uint8_t* buffer, int x, int y);
void vesa_write_str(uint8_t* buffer, int x, int y, const char* data, int color);
void vesa_fill(uint8_t* buffer, unsigned char color);

extern uint8_t forman[76800];

void vesa_init();


void vesa_put_pixel(uint8_t* buffer, int x,int y, unsigned char color);

inline void putpixel(uint8_t* buffer, int x,int y, unsigned char color, int pitch) {

    uint8_t* pixel_offset = (uint8_t*) (y * pitch + (x * (vbe_info->bpp/8)) + buffer);
    *pixel_offset = color;
}

inline unsigned char* getpixel(uint8_t* buffer, int x,int y, int pitch) {

    uint8_t* pixel_offset = (uint8_t*) (y * pitch + (x * (vbe_info->bpp/8)) + buffer);
    return pixel_offset;
}

/**
 * The vesa_* helpers draw into screen-sized buffers: the composition buffer, the framebuffer or the window server's
 * background. They skip whatever falls outside the screen, so a window partly off screen is cut off instead of
 * writing past the buffer into the kernel heap (#4). putpixel itself isn't clipped: it also draws into windows'
 * own buffers, which have their own size, and the kernel_gfx_* functions check against that.
 */
static inline int vesa_on_screen(int x, int y)
{
    return x >= 0 && y >= 0 && x < vbe_info->width && y < vbe_info->height;
}

/* The lines clamp their range once, rather than checking each pixel, since compositing draws them constantly */
static inline int vesa_clip_start(int start)
{
    return start < 0 ? 0 : start;
}

static inline int vesa_clip_end(int end, int limit)
{
    return end > limit ? limit : end;
}

/* Stripes are picked by absolute coordinate, so clamping the range keeps the pattern where it was */
inline void vesa_striped_line_horizontal(uint8_t* buffer, int x, int y, int length, int color, int stripe_width)
{
    if (y < 0 || y >= vbe_info->height) return;
    for (int i = vesa_clip_start(x); i < vesa_clip_end(x+length, vbe_info->width); i++)
        if (i % stripe_width == 0)
            putpixel(buffer, i, y, color, vbe_info->pitch);
}

inline void vesa_line_horizontal(uint8_t* buffer, int x, int y, int length, int color)
{
    if (y < 0 || y >= vbe_info->height) return;
    for (int i = vesa_clip_start(x); i < vesa_clip_end(x+length, vbe_info->width); i++)
        putpixel(buffer, i, y, color, vbe_info->pitch);
}

inline void vesa_striped_line_vertical(uint8_t* buffer, int x, int y, int length, int color, int stripe_width)
{
    if (x < 0 || x >= vbe_info->width) return;
    for (int i = vesa_clip_start(y); i < vesa_clip_end(y+length, vbe_info->height); i++)
        if (i % stripe_width == 0)
            putpixel(buffer, x, i, color, vbe_info->pitch);
}

inline void vesa_line_vertical(uint8_t* buffer, int x, int y, int length, int color)
{
    if (x < 0 || x >= vbe_info->width) return;
    for (int i = vesa_clip_start(y); i < vesa_clip_end(y+length, vbe_info->height); i++)
        putpixel(buffer, x, i, color, vbe_info->pitch);
}

void vesa_fillrect(uint8_t* buffer, int x, int y, int w, int h, int color);

int vesa_printf(uint8_t* buffer, int32_t x, int32_t y, int color, char* fmt, ...);
void vesa_inner_box(uint8_t* buffer, int x, int y, int w, int h, color_t color);
void vesa_init();

void vesa_put_icon16(uint8_t* buffer, int x, int y);
void vesa_put_icon32(uint8_t* buffer, int x, int y);
void vga_load_palette(void);

/* Position of the arrow's tip within the 16x16 cursor icon, where clicks land */
#define VESA_CURSOR_HOTSPOT_X 1
#define VESA_CURSOR_HOTSPOT_Y 2

#endif /* VBE_H */
