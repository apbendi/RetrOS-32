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

static struct vbe_mode_info_structure __vbe_info;
struct vbe_mode_info_structure* vbe_info = &__vbe_info;

/* The mouse cursor: palette indexes, with COLOR_TRANSPARENT around the arrow */
uint8_t cursor[16][16] = {
    {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,90,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,90,90,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,90,90,90,0x07,0x07,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,90,0x07,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,90,0x07,0xff,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,0x07,0xff,0xff,0xff,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,0xff,0xff,0xff,0xff,90,90,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,0xff,0xff,0xff,0xff,0xff,90,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
    ,{0xff,0xff,0xff,0xff,0xff,0xff,0x07,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
};

/**
 * The standard VGA palette, which vga_load_palette loads: each entry's red, green and blue, from 0 to 63, as the DAC
 * takes them. Every color in RetrOS, drawn or stored, is an index into it
 */
static const uint8_t vga_palette[256][3] = {
    /* The 16 EGA colors */
    {0, 0, 0}, {0, 0, 42}, {0, 42, 0}, {0, 42, 42}, {42, 0, 0}, {42, 0, 42}, {42, 21, 0}, {42, 42, 42},
    {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63}, {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63},
    /* A gray ramp, black to white */
    {0, 0, 0}, {5, 5, 5}, {8, 8, 8}, {11, 11, 11}, {14, 14, 14}, {17, 17, 17}, {20, 20, 20}, {24, 24, 24},
    {28, 28, 28}, {32, 32, 32}, {36, 36, 36}, {40, 40, 40}, {45, 45, 45}, {50, 50, 50}, {56, 56, 56}, {63, 63, 63},
    /**
     * 24 hues, from blue through magenta, red, yellow, green and cyan, at full brightness: at high saturation, then
     * moderate, then low
     */
    {0, 0, 63}, {16, 0, 63}, {31, 0, 63}, {47, 0, 63}, {63, 0, 63}, {63, 0, 47}, {63, 0, 31}, {63, 0, 16},
    {63, 0, 0}, {63, 16, 0}, {63, 31, 0}, {63, 47, 0}, {63, 63, 0}, {47, 63, 0}, {31, 63, 0}, {16, 63, 0},
    {0, 63, 0}, {0, 63, 16}, {0, 63, 31}, {0, 63, 47}, {0, 63, 63}, {0, 47, 63}, {0, 31, 63}, {0, 16, 63},
    {31, 31, 63}, {39, 31, 63}, {47, 31, 63}, {55, 31, 63}, {63, 31, 63}, {63, 31, 55}, {63, 31, 47}, {63, 31, 39},
    {63, 31, 31}, {63, 39, 31}, {63, 47, 31}, {63, 55, 31}, {63, 63, 31}, {55, 63, 31}, {47, 63, 31}, {39, 63, 31},
    {31, 63, 31}, {31, 63, 39}, {31, 63, 47}, {31, 63, 55}, {31, 63, 63}, {31, 55, 63}, {31, 47, 63}, {31, 39, 63},
    {45, 45, 63}, {49, 45, 63}, {54, 45, 63}, {58, 45, 63}, {63, 45, 63}, {63, 45, 58}, {63, 45, 54}, {63, 45, 49},
    {63, 45, 45}, {63, 49, 45}, {63, 54, 45}, {63, 58, 45}, {63, 63, 45}, {58, 63, 45}, {54, 63, 45}, {49, 63, 45},
    {45, 63, 45}, {45, 63, 49}, {45, 63, 54}, {45, 63, 58}, {45, 63, 63}, {45, 58, 63}, {45, 54, 63}, {45, 49, 63},
    /* The same at medium brightness */
    {0, 0, 28}, {7, 0, 28}, {14, 0, 28}, {21, 0, 28}, {28, 0, 28}, {28, 0, 21}, {28, 0, 14}, {28, 0, 7},
    {28, 0, 0}, {28, 7, 0}, {28, 14, 0}, {28, 21, 0}, {28, 28, 0}, {21, 28, 0}, {14, 28, 0}, {7, 28, 0},
    {0, 28, 0}, {0, 28, 7}, {0, 28, 14}, {0, 28, 21}, {0, 28, 28}, {0, 21, 28}, {0, 14, 28}, {0, 7, 28},
    {14, 14, 28}, {17, 14, 28}, {21, 14, 28}, {24, 14, 28}, {28, 14, 28}, {28, 14, 24}, {28, 14, 21}, {28, 14, 17},
    {28, 14, 14}, {28, 17, 14}, {28, 21, 14}, {28, 24, 14}, {28, 28, 14}, {24, 28, 14}, {21, 28, 14}, {17, 28, 14},
    {14, 28, 14}, {14, 28, 17}, {14, 28, 21}, {14, 28, 24}, {14, 28, 28}, {14, 24, 28}, {14, 21, 28}, {14, 17, 28},
    {20, 20, 28}, {22, 20, 28}, {24, 20, 28}, {26, 20, 28}, {28, 20, 28}, {28, 20, 26}, {28, 20, 24}, {28, 20, 22},
    {28, 20, 20}, {28, 22, 20}, {28, 24, 20}, {28, 26, 20}, {28, 28, 20}, {26, 28, 20}, {24, 28, 20}, {22, 28, 20},
    {20, 28, 20}, {20, 28, 22}, {20, 28, 24}, {20, 28, 26}, {20, 28, 28}, {20, 26, 28}, {20, 24, 28}, {20, 22, 28},
    /* The same at low brightness */
    {0, 0, 16}, {4, 0, 16}, {8, 0, 16}, {12, 0, 16}, {16, 0, 16}, {16, 0, 12}, {16, 0, 8}, {16, 0, 4},
    {16, 0, 0}, {16, 4, 0}, {16, 8, 0}, {16, 12, 0}, {16, 16, 0}, {12, 16, 0}, {8, 16, 0}, {4, 16, 0},
    {0, 16, 0}, {0, 16, 4}, {0, 16, 8}, {0, 16, 12}, {0, 16, 16}, {0, 12, 16}, {0, 8, 16}, {0, 4, 16},
    {8, 8, 16}, {10, 8, 16}, {12, 8, 16}, {14, 8, 16}, {16, 8, 16}, {16, 8, 14}, {16, 8, 12}, {16, 8, 10},
    {16, 8, 8}, {16, 10, 8}, {16, 12, 8}, {16, 14, 8}, {16, 16, 8}, {14, 16, 8}, {12, 16, 8}, {10, 16, 8},
    {8, 16, 8}, {8, 16, 10}, {8, 16, 12}, {8, 16, 14}, {8, 16, 16}, {8, 14, 16}, {8, 12, 16}, {8, 10, 16},
    {11, 11, 16}, {12, 11, 16}, {13, 11, 16}, {15, 11, 16}, {16, 11, 16}, {16, 11, 15}, {16, 11, 13}, {16, 11, 12},
    {16, 11, 11}, {16, 12, 11}, {16, 13, 11}, {16, 15, 11}, {16, 16, 11}, {15, 16, 11}, {13, 16, 11}, {12, 16, 11},
    {11, 16, 11}, {11, 16, 12}, {11, 16, 13}, {11, 16, 15}, {11, 16, 16}, {11, 15, 16}, {11, 13, 16}, {11, 12, 16},
    /* Black */
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
};

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

void vesa_put_icon16(uint8_t* buffer, int x, int y)
{
    for (int l = 0; l < 16; l++) {

        for (int i = 0; i < 16; i++) {
            if (cursor[l][i] != COLOR_TRANSPARENT) {
                vesa_put_pixel(buffer, (x)+i,  (y)+l, cursor[l][i]);
            }
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

/* Clamps its range to the screen once, since compositing fills rectangles constantly */
void vesa_fillrect(uint8_t* buffer, int x, int y, int w, int h, int color){
    int i, j;
    int x_end = vesa_clip_end(x+w, vbe_info->width);
    int y_end = vesa_clip_end(y+h, vbe_info->height);
    for (j = vesa_clip_start(y); j < y_end; j++)
        for (i = vesa_clip_start(x); i < x_end; i++)
            putpixel(buffer, i, j, color, vbe_info->pitch);
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
