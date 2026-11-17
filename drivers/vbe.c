/**
 * @file vbe.c
 * @author Joe Bayer (joexbayer)
 * @brief VESA / VBE code
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <vbe.h>
#include <colors.h>
#include <libc.h>
#include <font8.h>
#include <args.h>

#include <arch/io.h>
#include <palette.h>
#include <images.h>
#include <terminal.h>
#include <ksyms.h>

static struct vbe_mode_info_structure __vbe_info;
struct vbe_mode_info_structure* vbe_info = &__vbe_info;

/* No narrower than the screen, whatever its size: the helpers clamp to the screen as well */
#define VESA_CLIP_NONE 0x7FFFFFFF
struct vesa_clip vesa_clip = { 0, 0, VESA_CLIP_NONE, VESA_CLIP_NONE };

/* Narrows where the vesa_* helpers draw to x0, y0 up to x1, y1, clamped to the screen */
void vesa_set_clip(int x0, int y0, int x1, int y1)
{
    vesa_clip.x0 = x0 < 0 ? 0 : x0;
    vesa_clip.y0 = y0 < 0 ? 0 : y0;
    vesa_clip.x1 = x1;
    vesa_clip.y1 = y1;
}

void vesa_reset_clip(void)
{
    vesa_clip.x0 = 0;
    vesa_clip.y0 = 0;
    vesa_clip.x1 = VESA_CLIP_NONE;
    vesa_clip.y1 = VESA_CLIP_NONE;
}

/* The mouse cursor is images/cursor.bmp, whose size vbe.h gives so the window server can size buffers with it */
_Static_assert(IMAGE_CURSOR_WIDTH == VESA_CURSOR_WIDTH && IMAGE_CURSOR_HEIGHT == VESA_CURSOR_HEIGHT,
    "VESA_CURSOR_WIDTH and VESA_CURSOR_HEIGHT must match images/cursor.bmp");

/**
 * The VGA DAC's ports. With PALETTE_MASK at 0xFF, all 8 bits of a pixel pick its entry. To load entries, write the
 * first one's index to PALETTE_WRITE, then each one's red, green and blue to PALETTE_DATA
 */
#define PALETTE_MASK 0x3C6
#define PALETTE_WRITE 0x3C8
#define PALETTE_DATA 0x3C9

/**
 * Loads the standard VGA palette, so colors don't depend on what the video BIOS, or GRUB, set up. Only for graphics
 * modes: text mode maps its 16 colors to other entries. Assumes the DAC takes 6 bits a channel, as it does unless
 * VBE function 08h changes it
 */
void vga_load_palette(void)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_WRITE, 0);
    for (int i = 0; i < 256; i++) {
        outportb(PALETTE_DATA, vga_palette[i][0]);
        outportb(PALETTE_DATA, vga_palette[i][1]);
        outportb(PALETTE_DATA, vga_palette[i][2]);
    }
}

/* http://www.piclist.com/tecHREF/datafile/charset/extractor/charset_extractor.html */

unsigned short b8to16(unsigned char c)
{
    return (((unsigned short)c)<<8 ) | c;
}

/* Skips pixels off the screen, like every vesa_* helper (#4) */
void vesa_put_pixel(uint8_t* buffer, int x,int y, unsigned char color)
{
    if(vesa_on_screen(x, y)){
        putpixel(buffer, x , y, color, vbe_info->pitch);
    }
}

void vesa_put_block(uint8_t* buffer, unsigned char c, int x, int y, int color) 
{
    for (int l = 0; l < 8; l++) {
        for (int i = 8; i >= 0; i--) {
            if (font8x8_block[c][l] & (1 << i)) {
                vesa_put_pixel(buffer, (x)+i,  (y)+l, color);
            }
        }
    }
}

void vesa_put_char(uint8_t* buffer, unsigned char c, int x, int y, int color) 
{
    /* font8x8_basic has only the 128 ASCII characters; anything above draws as a blank (#83) */
    if(c >= 128) return;

    for (int l = 0; l < 8; l++) {
        for (int i = 8; i >= 0; i--) {
            if (font8x8_basic[c][l] & (1 << i)) {
                vesa_put_pixel(buffer, (x)+i,  (y)+l, color);
            }
        }
    }
}

void vesa_put_char16(uint8_t* buffer, unsigned char c, int x, int y, int color)
{
    /* font8x8_basic has only the 128 ASCII characters; anything above draws as a blank (#83) */
    if(c >= 128) return;

    for (int l = 0; l < 16; l++) {
        for (int i = 15; i >= 0; i--) {
            if (font8x8_basic[c][l/2] & (1 << (i/2))) {
                vesa_put_pixel(buffer, (x)+i,  (y)+l, color);
            }
        }
    }
}

/*
    Possible icons colors prefix:
    18, 
*/

/**
 * Draws row l of the mouse cursor, with its left edge at column x, over pixels, which holds the screen's columns from
 * first up to last. The window server puts the cursor over the screen as it copies it, never into a buffer (#163).
 */
void vesa_put_cursor_row(uint8_t* pixels, int first, int last, int x, int l)
{
    for (int i = 0; i < VESA_CURSOR_WIDTH; i++) {
        uint8_t pixel = cursor.pixels[l*VESA_CURSOR_WIDTH + i];
        if (pixel != COLOR_TRANSPARENT && x+i >= first && x+i < last) {
            pixels[x+i-first] = pixel;
        }
    }
}

void vesa_write(uint8_t* buffer, int x, int y, const char* data, int size, int color)
{
	for (int i = 0; i < size; i++)
		vesa_put_char(buffer, data[i], x+(i*PIXELS_PER_CHAR), y, color);
}

void vesa_write_str(uint8_t* buffer, int x, int y, const char* data, int color)
{
	vesa_write(buffer, x, y, data, strlen(data), color);
}

void vesa_inner_box(uint8_t* buffer, int x, int y, int w, int h, color_t color)
{
    vesa_fillrect(buffer, x, y, w, h, color);

    vesa_line_horizontal(buffer, x, y, w, 31);
    vesa_line_horizontal(buffer, x, y+h, w, COLOR_VGA_MEDIUM_DARK_GRAY+5);

    vesa_line_vertical(buffer, x, y, h, 31);
    vesa_line_vertical(buffer, x+w, y, h, COLOR_VGA_MEDIUM_DARK_GRAY+5);
}

void vesa_fill(uint8_t* buffer, unsigned char color)
{
    for (int i = 0; i < vbe_info->height; i++)
        for (int j = 0; j < vbe_info->width; j++)
            putpixel(buffer, j, i, color, vbe_info->pitch);
}

/* Clamps its range to the clip rectangle once, and fills a row at a time, since compositing fills rectangles constantly */
void vesa_fillrect(uint8_t* buffer, int x, int y, int w, int h, int color){
    int x_start = vesa_clip_x_start(x);
    int x_end = vesa_clip_x_end(x+w);
    int y_end = vesa_clip_y_end(y+h);
    if (x_end <= x_start) return;
    for (int j = vesa_clip_y_start(y); j < y_end; j++)
        memset(buffer + j*vbe_info->pitch + x_start, color, x_end - x_start);
}

#define GFX_MAX_FMT_STR_SIZE 50

int vesa_printf(uint8_t* buffer, int32_t x, int32_t y, int color, char* fmt, ...)
{
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[GFX_MAX_FMT_STR_SIZE];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {
		switch (*fmt)
		{
			case '%':
				memset(str, 0, GFX_MAX_FMT_STR_SIZE);
				switch (*(fmt+1))
				{
					case 'd':
					case 'i': ;
						num = va_arg(args, int);
						itoa(num, str);
						vesa_write_str(buffer, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);
						break;
                    case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);
						vesa_write_str(buffer, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);

                        if(strlen(str) < 3){
                            int pad = 3-strlen(str);
                            for (int i = 0; i < pad; i++){
                                vesa_put_char(buffer, ' ', x+(x_offset*PIXELS_PER_CHAR), y, color);
                                x_offset++;
                            }
                        }
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						vesa_write_str(buffer, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						if (str_arg == NULL) str_arg = "(null)";
						vesa_write_str(buffer, x+(x_offset*PIXELS_PER_CHAR), y, str_arg, color);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						vesa_put_char(buffer, char_arg, x+(x_offset*PIXELS_PER_CHAR), y, color);
						x_offset++;
						break;
					default:
                        break;
				}
				fmt++;
				break;
			case '\n':
				y += 8;
				written += x_offset;
				x_offset = 0;
				break;
			default:  
				vesa_put_char(buffer, *fmt, x+(x_offset*PIXELS_PER_CHAR), y, color);
				x_offset++;
                written++;
			}
        fmt++;
    }
	written += x_offset;
	return written;
}

/* VBE's memory models, by number */
static const char* vbe_memory_models[] = {
    "text", "CGA", "Hercules", "planar", "packed", "non-chain 4", "direct", "YUV"
};

/* Mode attributes: the hardware supports it, it's a graphics mode, and it has a linear framebuffer */
#define VBE_MODE_SUPPORTED  0x0001
#define VBE_MODE_GRAPHICS   0x0010
#define VBE_MODE_LINEAR     0x0080

/**
 * Lists the video BIOS's modes, as the bootloader recorded them before its menu: each mode's number, size, bits
 * per pixel, memory model and bytes per row, and whether it has a linear framebuffer, which RetrOS draws into.
 * RetrOS uses 8-bit packed modes. With a number, only modes with that many bits per pixel: vbemodes 8. The mode in
 * use is marked with *.
 */
void vbemodes(int argc, char** argv)
{
    struct vbe_mode_table* table = (struct vbe_mode_table*) VBE_MODE_TABLE_ADDRESS;
    if(table->magic != VBE_MODE_TABLE_MAGIC){
        twritef("The bootloader didn't list the video modes.\n");
        return;
    }
    if(table->status != VBE_STATUS_OK){
        twritef("The video BIOS has no VBE: function 4F00h returned 0x%x.\n", table->status);
        return;
    }

    int bpp = argc > 1 ? atoi(argv[1]) : 0;

    twritef("VBE %d.%d, %d KB of video memory, %d modes\n",
        table->version >> 8, table->version & 0xFF, table->memory * 64, table->count);

    for(int i = 0; i < table->count && i < VBE_MODE_TABLE_MAX; i++){
        struct vbe_mode_entry* mode = &table->modes[i];
        if(mode->status != VBE_STATUS_OK){
            if(bpp == 0) twritef("  0x%x: no information (0x%x)\n", mode->mode, mode->status);
            continue;
        }
        if(bpp != 0 && mode->bpp != bpp) continue;

        int in_use = mode->width == vbe_info->width && mode->height == vbe_info->height && mode->bpp == vbe_info->bpp;
        int linear_pitch = mode->linear_pitch != 0 && mode->linear_pitch != mode->pitch;

        twritef("%s0x%x %dx%d %d-bit %s, pitch %d", in_use ? "* " : "  ", mode->mode, mode->width, mode->height,
            mode->bpp, mode->memory_model < 8 ? vbe_memory_models[mode->memory_model] : "other", mode->pitch);
        if(linear_pitch) twritef(" (%d linear)", mode->linear_pitch);
        twritef("%s%s%s\n",
            mode->attributes & VBE_MODE_LINEAR ? ", linear" : "",
            mode->attributes & VBE_MODE_GRAPHICS ? "" : ", text",
            mode->attributes & VBE_MODE_SUPPORTED ? "" : ", not supported"
        );
    }
}
EXPORT_KSYMBOL(vbemodes);
