/**
 * @file gfxlib.c
 * @author Joe Bayer (joexbayer)
 * @brief GFX library for rending to a framebuffer.
 * @version 0.1
 * @date 2023-01-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <gfx/gfxlib.h>
#include <gfx/events.h>
#include <pcb.h>
#include <font8.h>
#include <vbe.h>
#include <args.h>
#include <vbe.h>
#include <serial.h>
#include <scheduler.h>
#include <kutils.h>

/**
 * Every function here that writes a window's pixels holds w->buffer_lock from its bounds check
 * through its last pixel. A resize replaces the buffer on the window server thread, and the lock
 * keeps it from doing that in the middle of a draw. The lock isn't reentrant, so a function
 * that holds it must not call another one that takes it.
 */

/* Programs copy events in and out by this size, and flags took padding that was already there */
_Static_assert(sizeof(struct gfx_event) == 6, "struct gfx_event must stay 6 bytes");

int gfx_get_window_width()
{
	return $process->current->gfx_window->inner_width;
}

int gfx_get_window_height()
{
	return $process->current->gfx_window->inner_height;
}

int gfx_push_event(struct window* w, struct gfx_event* e)
{
	ERR_ON_NULL(w);

	SPINLOCK(w,  {
		/* A drag sends a move every frame, so a move replaces one at the end of the queue, rather than pushing older events out */
		int last = (w->events.head + GFX_MAX_EVENTS - 1) % GFX_MAX_EVENTS;
		if(e->event == GFX_EVENT_MOUSE_MOVE && w->events.head != w->events.tail && w->events.list[last].event == GFX_EVENT_MOUSE_MOVE){
			memcpy(&w->events.list[last], e, sizeof(*e));
		} else {
			int next_head = (w->events.head + 1) % GFX_MAX_EVENTS;
			/* When full, drop the oldest event. Letting head catch up to tail would make the queue look empty. */
			if(next_head == w->events.tail){
				w->events.tail = (w->events.tail + 1) % GFX_MAX_EVENTS;
			}

			memcpy(&w->events.list[w->events.head], e, sizeof(*e));
			w->events.head = next_head;
		}
	});

	/* TODO: Check for a block reason */
	pcb_wake(w->owner);

	return 0;
}

/**
 * Takes the oldest event from w's queue, without waiting: for a thread reading a window that isn't its own, like a
 * program reading the keys typed in its shell's window.
 * @return 0, or -1 if there was none
 */
int gfx_window_get_event(struct window* w, struct gfx_event* event)
{
	ERR_ON_NULL(w);

	int ret = -1;
	SPINLOCK(w, {
		if(w->events.tail != w->events.head){
			memcpy(event, &w->events.list[w->events.tail], sizeof(struct gfx_event));
			w->events.tail = (w->events.tail + 1) % GFX_MAX_EVENTS;
			ret = 0;
		}
	});
	return ret;
}

int kernel_gfx_draw_contoured_box(struct window* w, int x, int y, int width, int height, color_t color) 
{
	kernel_gfx_draw_rectangle(w, x, y, width, height, color);
	kernel_gfx_draw_rectangle(w, x, y, width-1, 1, 31);
	kernel_gfx_draw_rectangle(w, x, y, 1, height, 31);
	kernel_gfx_draw_rectangle(w, x+width-1, y, 1, height, COLOR_VGA_MEDIUM_DARK_GRAY+5);
	kernel_gfx_draw_rectangle(w, x, y+height-1, width-1, 1, COLOR_VGA_MEDIUM_DARK_GRAY+5);
	kernel_gfx_draw_rectangle(w, x, y+height, width-1, 1, 31);

	return 0;
}

int gfx_button(int x, int y, int width, int height, char* name)
{
	return gfx_button_ext(x, y, width, height, name, 30);
}

int gfx_button_ext(int x, int y, int width, int height, char* name, color_t color)
{
	kernel_gfx_draw_rectangle($process->current->gfx_window, x, y, width, height, color);
	kernel_gfx_draw_rectangle($process->current->gfx_window, x, y, width-1, 1, 31);
	kernel_gfx_draw_rectangle($process->current->gfx_window, x, y, 1, height, 31);
	kernel_gfx_draw_rectangle($process->current->gfx_window, x+width-1, y, 1, height, COLOR_VGA_MEDIUM_DARK_GRAY+5);
	kernel_gfx_draw_rectangle($process->current->gfx_window, x, y+height-1, width-1, 1, COLOR_VGA_MEDIUM_DARK_GRAY+5);
	kernel_gfx_draw_rectangle($process->current->gfx_window, x, y+height, width-1, 1, 31);

	// /* center the text */
	// int text_width = strlen(name)*PIXELS_PER_CHAR;
	// int text_height = 8;
	// int text_x = x + (width/2) - (text_width/2);
	// int text_y = y + (height/2) - (text_height/2);

	kernel_gfx_draw_text($process->current->gfx_window, x+4, y+4, name, 0x0);

	return 0;
}

int gfx_put_icon16(const unsigned char icon[], int x, int y)
{
	spin_lock(&$process->current->gfx_window->buffer_lock);
	for (int l = 0; l < 16; l++) {

		for (int i = 0; i < 16; i++) {
			if (icon[l*16+i] != COLOR_TRANSPARENT) {

				if((x)+i < 0 || (y)+l < 0 || (x)+i >= $process->current->gfx_window->inner_width || (y)+l >= $process->current->gfx_window->inner_height)
					continue;
				putpixel($process->current->gfx_window->inner, (x)+i, (y)+l, icon[l*16+i], $process->current->gfx_window->pitch);
			}
		}
	}
	spin_unlock(&$process->current->gfx_window->buffer_lock);
	return 0;
}

int gfx_put_icon32(const unsigned char icon[], int x, int y)
{
	spin_lock(&$process->current->gfx_window->buffer_lock);
	for (int l = 0; l < 32; l++) {

		for (int i = 0; i < 32; i++) {
			if (icon[l*32+i] != COLOR_TRANSPARENT) {

				if((x)+i < 0 || (y)+l < 0 || (x)+i >= $process->current->gfx_window->inner_width || (y)+l >= $process->current->gfx_window->inner_height)
					continue;
				putpixel($process->current->gfx_window->inner, (x)+i, (y)+l, icon[l*32+i], $process->current->gfx_window->pitch);
			}
		}
	}
	spin_unlock(&$process->current->gfx_window->buffer_lock);
	return 0;
}

int gfx_event_loop(struct gfx_event* event, gfx_event_flag_t flags)
{

	ERR_ON_NULL($process->current->gfx_window);
	/**
	 * The gfx event loop is PCB specific,
	 * checks if there is an event if true return.
	 * Else block.
	 */
	while(1){
		if($process->current->gfx_window->events.tail == $process->current->gfx_window->events.head){
			/* A program killed while it waits exits when the call returns (pcb_kill) */
			if($process->current->kill_pending){
				return -1;
			}

			if(flags & GFX_EVENT_BLOCKING){
				/* FIXME: Should not global block, perhaps window manager block queue? */
				$process->current->state = BLOCKED;
				kernel_yield();
			} else {
				return -1;
			}
			continue;
		}
		
		SPINLOCK($process->current->gfx_window, {
			memcpy(event, &$process->current->gfx_window->events.list[$process->current->gfx_window->events.tail], sizeof(struct gfx_event));
			$process->current->gfx_window->events.tail = ($process->current->gfx_window->events.tail + 1) % GFX_MAX_EVENTS;
		});
		return 0;
	}
}

/**
 * @brief Draws a rectangle onto the inner framebufferfor currently running process.
 * 
 * @param w window to draw to
 * @param x coordinate
 * @param y coordiante
 * @param width of rectangle
 * @param height of rectangle
 * @param color
 * @return int 0 on success, less than 0 on error
 */
int kernel_gfx_draw_rectangle(struct window* w, int x, int y, int width, int height, color_t color)
{
	ERR_ON_NULL(w);

	spin_lock(&w->buffer_lock);

	/* only draw rectangle inside window */
	if(x < 0 || y < 0 || x+width > w->inner_width || y+height > w->inner_height){
		spin_unlock(&w->buffer_lock);
		return -2;
	}

	int i, j;
	for (j = y; j < (y+height); j++)
		for (i = x; i < (x+width); i++)
			putpixel(w->inner, i, j, color, w->pitch);

	spin_unlock(&w->buffer_lock);
	return 0;
}

int kernel_gfx_draw_pixel(struct window* w, int x, int y, color_t color)
{
	ERR_ON_NULL(w);

	spin_lock(&w->buffer_lock);

	/* only draw rectangle inside window */
	if(x < 0 || y < 0 || x >= w->inner_width || y >= w->inner_height){
		spin_unlock(&w->buffer_lock);
		return -2;
	}

	putpixel(w->inner, x, y, color, w->pitch);

	spin_unlock(&w->buffer_lock);
	return 0;
}

int kernel_gfx_draw_bitmap(struct window* w, int x, int y, int width, int height, const uint8_t* bitmap)
{
	ERR_ON_NULL(w);

	spin_lock(&w->buffer_lock);

	/* only draw rectangle inside window */
	if(x < 0 || y < 0 || x+width > w->inner_width || y+height > w->inner_height){
		spin_unlock(&w->buffer_lock);
		return -2;
	}

	int i, j;
	for (j = y; j < (y+height); j++)
		for (i = x; i < (x+width); i++)
			putpixel(w->inner, i, j, bitmap[(j-y)*width+(i-x)], w->pitch);

	spin_unlock(&w->buffer_lock);
	return 0;
}

/**
 * @brief Draws a character to the framebuffer of currently running process.
 * 
 * @param x coordinate
 * @param y coordiante
 * @param c character
 * @param color 
 * @return int 0 on success, less than 0 on error.
 */
int kernel_gfx_draw_char(struct window* w, int x, int y, unsigned char c, unsigned char color)
{

	ERR_ON_NULL(w);

	/* font8x8_basic has only the 128 ASCII characters; anything above draws as a blank (#83) */
	if(c >= 128) return 0;

	spin_lock(&w->buffer_lock);
	for (int l = 0; l < 8; l++) {
		for (int i = 8; i >= 0; i--) {
			if (font8x8_basic[c][l] & (1 << i)) {

				if((x)+i < 0 || (y)+l < 0 || (x)+i >= w->inner_width || (y)+l >= w->inner_height)
					continue;
				putpixel(w->inner, (x)+i, (y)+l, color, w->pitch);
			}
		}
	}
	spin_unlock(&w->buffer_lock);

	return 0;
}

void kernel_gfx_set_position(struct window* w, int x, int y)
{
	w->x = x;
	w->y = y;
}

void gfx_commit()
{
	if($process->current->gfx_window == NULL)
		return;
	$process->current->gfx_window->changed = 1;
}

/**
 * @brief kernel_gfx_draw_char wrapper for strings
 * 
 * @param x 
 * @param y 
 * @param str string to write.
 * @param color 
 * @return int 0 on success, less than 0 on error.
 */
int kernel_gfx_draw_text(struct window* w, int x, int y, const char* str, unsigned char color)
{
	ERR_ON_NULL(w);

	for (int i = 0; i < strlen(str); i++){
		if(x+(i*8) >= w->inner_width-10){
			y += 8;
			x = 0;
		}
		kernel_gfx_draw_char(w, x+(i*8), y, str[i], color);
	}

	return 0;
}

/* A title or header has to fit with its terminating 0, and a thread without a window has none to set */
int kernel_gfx_set_title(const char* title)
{
	if(title == NULL || $process->current->gfx_window == NULL || strlen(title) >= GFX_MAX_WINDOW_NAME_SIZE)
		return -1;
	
	memset($process->current->gfx_window->name, 0, GFX_MAX_WINDOW_NAME_SIZE);
	memcpy($process->current->gfx_window->name, title, strlen(title));

	return 0;
}

int kernel_gfx_set_header(const char* header)
{
	if(header == NULL || $process->current->gfx_window == NULL || strlen(header) >= GFX_MAX_WINDOW_NAME_SIZE)
		return -1;
	
	memset($process->current->gfx_window->header, 0, GFX_MAX_WINDOW_NAME_SIZE);
	memcpy($process->current->gfx_window->header, header, strlen(header));

	return 0;
}

#define ABS(N) ((N<0)?(-N):(N))

/* Line coordinates beyond this are rejected, so one line can't keep the pixel loop running for long */
#define GFX_LINE_MAX_COORD 4096
#define GFX_LINE_COORD_OK(v) ((v) >= -GFX_LINE_MAX_COORD && (v) <= GFX_LINE_MAX_COORD)

/* Writes one pixel of a window's content, skipping it if it's outside the window. The caller holds w->buffer_lock. */
static void __window_put_pixel(struct window* w, int x, int y, unsigned char color)
{
	if(x < 0 || y < 0 || x >= w->inner_width || y >= w->inner_height) return;
	putpixel(w->inner, x, y, color, w->pitch);
}

/**
plotLine(x0, y0, x1, y1)
	dx = abs(x1 - x0)
	sx = x0 < x1 ? 1 : -1
	dy = -abs(y1 - y0)
	sy = y0 < y1 ? 1 : -1
	error = dx + dy
	
	while true
		plot(x0, y0)
		if x0 == x1 && y0 == y1 break
		e2 = 2 * error
		if e2 >= dy
			if x0 == x1 break
			error = error + dy
			x0 = x0 + sx
		end if
		if e2 <= dx
			if y0 == y1 break
			error = error + dx
			y0 = y0 + sy
		end if
	end while
*/

void kernel_gfx_draw_line(struct window* w, int x0, int y0, int x1, int y1, unsigned char color)
{  
	/**
	 * x is the row and y the column (see the pixel write below), and callers rely on that.
	 * Every pixel is clipped to the window, so a line crossing an edge is cut off there
	 * instead of being dropped.
	 */
	if(!GFX_LINE_COORD_OK(x0) || !GFX_LINE_COORD_OK(y0) || !GFX_LINE_COORD_OK(x1) || !GFX_LINE_COORD_OK(y1))
		return;

	int dx, dy, sx, sy, error;
	int t1 = x1-x0;
	int t2 = y1-y0;
	dx= ABS(t1);
	sx = x0 < x1 ? 1 : -1;
	dy=-(ABS(t2));  
	sy = y0 < y1 ? 1 : -1;

	error = dx + dy;

	spin_lock(&w->buffer_lock);
	while(1)
	{
		/* Row x0, column y0 */
		__window_put_pixel(w, y0, x0, color);
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2*error;

		if( e2 >= dy){
			if (x0 == x1) break;
			error = error + dy;
			x0 = x0 + sx;
		}

		if( e2 <= dx){
			if (y0 == y1) break;
			error = error + dx;
			y0 = y0 + sy;
		}
	}
	spin_unlock(&w->buffer_lock);
}

static void kernel_gfx_draw_circle_helper(struct window* w, int xc, int yc, int x, int y, unsigned char color, bool_t fill)
{
    // Draw the circle outline
    __window_put_pixel(w, xc + x, yc + y, color);
    __window_put_pixel(w, xc - x, yc + y, color);
    __window_put_pixel(w, xc + x, yc - y, color);
    __window_put_pixel(w, xc - x, yc - y, color);
    __window_put_pixel(w, xc + y, yc + x, color);
    __window_put_pixel(w, xc - y, yc + x, color);
    __window_put_pixel(w, xc + y, yc - x, color);
    __window_put_pixel(w, xc - y, yc - x, color);

    // Fill the circle if the flag is set
    if (fill)
    {
        for (int i = xc - x; i <= xc + x; i++) {
            for (int j = yc - y + 1; j < yc + y; j++) {
                __window_put_pixel(w, i, j, color);
            }
        }

        for (int i = xc - y; i <= xc + y; i++) {
            for (int j = yc - x + 1; j < yc + x; j++) {
                __window_put_pixel(w, i, j, color);
            }
        }
    }
}

// using Bresenham's algorithm
void kernel_gfx_draw_circle(struct window* w, int xc, int yc, int r, unsigned char color, bool_t fill)
{
    int x = 0, y = r;
    int d = 3 - 2 * r;

    spin_lock(&w->buffer_lock);
    kernel_gfx_draw_circle_helper(w, xc, yc, x, y, color, fill);

    while (y >= x)
    {
        x++;

        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
        {
            d = d + 4 * x + 6;
        }

        kernel_gfx_draw_circle_helper(w, xc, yc, x, y, color, fill);
    }
    spin_unlock(&w->buffer_lock);
}


int kernel_gfx_draw_section(int x, int y, int width, int height, const char* name)
{

	return 0;
}

#define GFX_MAX_FMT 50
int kernel_gfx_draw_format_text(struct window* w, int x, int y, unsigned char color, const char* fmt, ...)
{
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[GFX_MAX_FMT];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {

		if(x+(x_offset*PIXELS_PER_CHAR) >= w->inner_width){
			y += 8;
			written += x_offset;
			x_offset = 0;
		}

		switch (*fmt)
		{
			case '%':
				memset(str, 0, GFX_MAX_FMT);
				switch (*(fmt+1))
				{
					case 'd':
						num = va_arg(args, int);
						itoa(num, str);
						kernel_gfx_draw_text(w, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);
						break;
					case 'i':
						num = va_arg(args, int);
						unsigned int bytes[4];
						bytes[0] = (num >> 24) & 0xFF;
						bytes[1] = (num >> 16) & 0xFF;
						bytes[2] = (num >> 8) & 0xFF;
						bytes[3] = num & 0xFF;
						x_offset +=  kernel_gfx_draw_format_text(w, x+(x_offset*PIXELS_PER_CHAR), y, color, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
						break;
					case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);

						if(strlen(str) < 6){
							int pad = 6-strlen(str);
							for (int i = 0; i < pad; i++){
								kernel_gfx_draw_char(w, x+(x_offset*PIXELS_PER_CHAR), y,'0', color);
								x_offset++;
							}
						}
								dbgprintf("OFFSET %d\n", strlen(str));

						kernel_gfx_draw_text(w, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						kernel_gfx_draw_text(w, x+(x_offset*PIXELS_PER_CHAR), y, str, color);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						kernel_gfx_draw_text(w, x+(x_offset*PIXELS_PER_CHAR), y, str_arg, color);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						kernel_gfx_draw_char(w, x+(x_offset*PIXELS_PER_CHAR), y, char_arg, color);
						x_offset++;
						break;
					default:
						kernel_gfx_draw_char(w, x+(x_offset*PIXELS_PER_CHAR), y, *fmt, color);
						fmt--;
						x_offset++;
						written++;
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

				kernel_gfx_draw_char(w, x+(x_offset*PIXELS_PER_CHAR), y, *fmt, color);
				x_offset++;
				written++;
			}
		fmt++;
	}
	written += x_offset;
	gfx_commit();
	return written;
}
