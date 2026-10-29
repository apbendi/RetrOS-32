/**
 * @file graphics.c
 * @author Joe Bayer (joexbayer)
 * @brief Userspace graphics library.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <lib/graphics.h>
#include <lib/syscall.h>
#include <fs/fs_types.h>
#include <bmp.h>
#define ERRORS_CODES_ONLY
#include <errors.h>
#include <colors.h>
#include <libc.h>
#include <args.h>

#ifdef __cplusplus
extern "C" {
#endif

int gfx_draw_char(int x, int y, char data, unsigned char color)
{
    struct gfx_char c = {
        .color = color,
        .data = data,
        .x = x,
        .y = y
    };
    gfx_draw_syscall(GFX_DRAW_CHAR_OPT, &c, 0);

    return 0;
}

int gfx_draw_pixel(int x, int y, unsigned char color)
{
	struct gfx_pixel p = {
		.x = x,
		.y = y,
		.color = color
	};

	gfx_draw_syscall(GFX_DRAW_PIXEL, &p, 0);

	return 0;
}

int gfx_draw_image(int x, int y, const struct image* image)
{
	struct gfx_image draw = {
		.x = x,
		.y = y,
		.image = *image
	};

	return gfx_draw_syscall(GFX_DRAW_IMAGE, &draw, 0);
}

/* The image for one that can't be loaded: a white box, outlined in dark gray, with a red X, like the kernel's */
static unsigned char placeholder[16 * 16];

static const unsigned char* placeholder_pixels(void)
{
	if(placeholder[0] == 0){
		for (int y = 0; y < 16; y++){
			for (int x = 0; x < 16; x++){
				unsigned char color = COLOR_TRANSPARENT;
				if(x >= 1 && x <= 14 && y >= 1 && y <= 14){
					color = x == 1 || x == 14 || y == 1 || y == 14 ? 8 : 15;
				}
				/* The X: two pixels wide, from row 4 to 11 */
				int d = y < 8 ? y : 15 - y;
				if(y >= 4 && y <= 11 && (x == d || x == d + 1 || x == 15 - d || x == 14 - d)){
					color = 12;
				}
				placeholder[y * 16 + x] = color;
			}
		}
	}
	return placeholder;
}

int gfx_load_icon(const char* name, struct image* image)
{
	image->width = 16;
	image->height = 16;
	image->pixels = placeholder_pixels();

	char path[FS_PATH_MAX];
	int dir = strlen(ICONS_DIR);
	int len = strlen(name);
	if(dir + len + 5 > FS_PATH_MAX){
		return -ERROR_INVALID_ARGUMENTS;
	}
	memcpy(path, ICONS_DIR, dir);
	memcpy(path + dir, name, len);
	memcpy(path + dir + len, ".bmp", 5);

	struct fs_dirent info;
	int ret = stat(path, &info);
	if(ret < 0){
		return ret;
	}
	if(info.size == 0 || info.size > BMP_FILE_MAX){
		return -ERROR_BAD_IMAGE;
	}

	unsigned char* data = malloc(info.size);
	if(data == NULL){
		return -ERROR_ALLOC;
	}
	int fd = open(path, FS_FILE_FLAG_READ);
	if(fd < 0){
		free(data);
		return fd;
	}
	int got = 0;
	while (got < (int)info.size){
		int n = read(fd, data + got, info.size - got);
		if(n <= 0){
			break;
		}
		got += n;
	}
	fclose(fd);

	struct bmp_info bmp;
	if(got != (int)info.size || bmp_parse(data, got, &bmp) < 0){
		free(data);
		return -ERROR_BAD_IMAGE;
	}
	unsigned char* pixels = malloc(bmp.width * bmp.height);
	if(pixels == NULL){
		free(data);
		return -ERROR_ALLOC;
	}
	bmp_decode(data, &bmp, pixels);
	free(data);

	image->width = bmp.width;
	image->height = bmp.height;
	image->pixels = pixels;
	return 0;
}

void gfx_free_icon(struct image* image)
{
	if(image->pixels != NULL && image->pixels != placeholder){
		free((void*)image->pixels);
	}
	image->pixels = NULL;
}

int gfx_draw_circle(int x, int y, int r, unsigned char color, char fill)
{
	struct gfx_circle c = {
		.x = x,
		.y = y,
		.r = r,
		.fill = fill,
		.color = color
	};

	gfx_draw_syscall(GFX_DRAW_CIRCLE_OPT, &c, 0);

	return 0;
}

int gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color)
{
	/* The kernel draws x as the row and y as the column, so swap them */
	struct gfx_line line = {
		.x0 = y0,
		.y0 = x0,
		.y1 = x1,
		.x1 = y1,
		.color = color
	};

	gfx_draw_syscall(GFX_DRAW_LINE_OPT, &line, 0);

	return 0;
}

int gfx_draw_rectangle(int x, int y, int width, int height, unsigned char color)
{
    struct gfx_rectangle rect = {
		.color = color,
		.x = x,
		.y = y,
		.width = width,
		.height = height
	};

    gfx_draw_syscall(GFX_DRAW_RECTANGLE_OPT, &rect, 0);

    return 0;
}

int gfx_draw_text(int x, int y, const char* text, unsigned char color)
{
    struct gfx_char c = {
        .color = color,
        .data = 0,
        .x = x,
        .y = y
    };

    int len = strlen(text);
    for (int i = 0; i < len; i++)
    {
        c.data = text[i];
        gfx_draw_syscall(GFX_DRAW_CHAR_OPT, &c, 0);
        c.x += 8;
    }
    
    return 0;
}

int gfx_get_event(struct gfx_event* event, gfx_event_flag_t flags)
{
	return gfx_draw_syscall(GFX_EVEN_LOOP_OPT, event, flags);
}

#define GFX_MAX_FMT 50
int gfx_draw_format_text(int x, int y, unsigned char color, const char* fmt, ...)
{
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[GFX_MAX_FMT];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {
		switch (*fmt)
		{
			case '%':
				memset(str, 0, GFX_MAX_FMT);
				switch (*(fmt+1))
				{
					case 'd':
					case 'i': ;
						num = va_arg(args, int);
						itoa(num, str);
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);
						break;
                    case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);

                        if(strlen(str) < 4){
                            int pad = 4-strlen(str);
                            for (int i = 0; i < pad; i++){
                                gfx_draw_char(x+(x_offset*8), y, '0', color);
                                x_offset++;
                            }
                        }
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						if (str_arg == NULL) str_arg = "(null)";
						gfx_draw_text(x+(x_offset*8), y, str_arg, color);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						gfx_draw_char(x+(x_offset*8), y, char_arg, color);
						x_offset++;
						break;
					default:
                        break;
				}
				fmt++;
				break;
			case '\n':
				y++;
				written += x_offset;
				x_offset = 0;
				break;
			default:  
				gfx_draw_char(x+(x_offset*8), y, *fmt, color);
				x_offset++;
                written++;
			}
        fmt++;
    }
	written += x_offset;
	return written;
}

#ifdef __cplusplus
}
#endif
