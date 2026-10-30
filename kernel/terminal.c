/**
 * @file terminal.c
 * @author Joe Bayer (joexbayer)
 * @brief Handles terminal input and currently.. UI drawing.
 * @version 0.2
 * @date 2022-06-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <terminal.h>
#include <args.h>
#include <colors.h>
#include <gfx/gfxlib.h>
#include <pcb.h>
#include <terminal.h>
#include <gfx/theme.h>
#include <gfx/events.h>
#include <conf.h>
#include <screen.h>
#include <memory.h>
#include <serial.h>
#include <vbe.h>
#include <scheduler.h>
#include <clipboard.h>
#include <gfx/composition.h>

#define MAX_FMT_STR_SIZE 50
#define TERMINAL_TEXTBUFFER_INITIAL_CAPACITY 1024
/* OLD */

int scan(ubyte_t* data, int size)
{
	if($process->current == NULL || $process->current->term == NULL) return -1;

	return $process->current->term->ops->scan($process->current->term, data, size);
}

/* The color a graphics terminal draws a character in: some characters are highlighted. It depends only on the
 * terminal and the character, so text drawn in pieces looks the same as text drawn all at once */
static color_t __terminal_syntax_color(struct terminal* term, unsigned char c)
{
	switch (c) {
		case '>':
		case '/':
		case '\\':
			return COLOR_VGA_MISC;
		case '"':
		case ':':
		case '-':
			return COLOR_VGA_GREEN;
		default:
			return term->text_color;
	}
}

static int __next_newline(void* _data)
{
	char* data = (char*) _data;

	while (*data != '\n'){
		data++;
	}
	data++;

	return (int)data - (int)_data;
}

/**
 * The selected text, from *start up to *end in the text buffer, whichever way it was selected, and within the text.
 * @return false if nothing is selected
 */
static bool_t __terminal_selection(struct terminal* term, int* start, int* end)
{
	if(term->sel_anchor < 0) return false;

	int anchor = term->sel_anchor < term->head ? term->sel_anchor : term->head;
	int moving = term->sel_end < term->head ? term->sel_end : term->head;
	*start = anchor < moving ? anchor : moving;
	*end = anchor < moving ? moving : anchor;
	return *start < *end;
}

/* Moves the selection with the text when count characters go from the text buffer's start. What went is no longer selected */
static void __terminal_selection_dropped(struct terminal* term, int count)
{
	if(term->sel_anchor < 0) return;

	term->sel_anchor = term->sel_anchor > count ? term->sel_anchor - count : 0;
	term->sel_end = term->sel_end > count ? term->sel_end - count : 0;
	if(term->sel_anchor == term->sel_end && !term->selecting){
		term->sel_anchor = -1;
	}
}

static void __terminal_remove_line(struct terminal* term)
{
	int skip = __next_newline(term->textbuffer);
	memmove(term->textbuffer, &term->textbuffer[skip], term->head-skip);
	term->head -= skip;
	/* What's drawn and what's selected move with the text */
	term->drawn_end -= skip;
	__terminal_selection_dropped(term, skip);
}

static void __terminal_scroll(struct terminal* term)
{
	/*if(term->tail == term->head)
		return;
		
	while(term->textbuffer[term->tail] != '\n' && term->head != term->tail){
		term->tail++;
	}
	term->tail++;*/

	__terminal_remove_line(term);
}

static int __terminal_grow_textbuffer(struct terminal* term, int min_capacity)
{
	if(term->textbuffer == NULL){
		return -1;
	}

	int new_size = term->textbuffer_size > 0 ? term->textbuffer_size : TERMINAL_TEXTBUFFER_INITIAL_CAPACITY;
	while(new_size <= min_capacity){
		new_size *= 2;
	}

	char* new_buffer = (char*) krealloc(term->textbuffer, term->textbuffer_size, new_size);
	if(new_buffer == NULL){
		warningf("[TERM] Failed to grow text buffer to %d bytes\n", new_size);
		return -1;
	}

	term->textbuffer = new_buffer;
	term->textbuffer_size = new_size;
	return 0;
}

/**
 * Copies what the terminal shows to the serial log a line at a time, as "term[<writer>]: <line>",
 * so the output of shell commands and apps can be read from the log. Every character a terminal
 * shows comes through __terminal_store_char, in graphics and text mode.
 */
static void __terminal_copy_to_serial(struct terminal* term, char c)
{
	if(c != '\n'){
		/* A zero byte would end the line early in serial_printf's %s, so it's logged as the blank the screen shows */
		term->serial_line[term->serial_line_length++] = c == 0 ? ' ' : c;
		if(term->serial_line_length < TERMINAL_SERIAL_LINE_SIZE-1) return;
	}
	term->serial_line[term->serial_line_length] = 0;

	/* One call, and serial_printf runs with interrupts off, so other threads' log lines can't split the line */
	serial_printf("\033[36mterm[%s]: \033[37m%s\n", $process->current != NULL ? $process->current->name : "?", term->serial_line);
	term->serial_line_length = 0;
}

/* Copies a line the terminal hasn't finished to the serial log, as when a program exits without ending its last line.
 * Otherwise it would reach the log only with whatever's written next, under that writer's name. */
void terminal_flush_serial(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	if(term->serial_line_length > 0){
		__terminal_copy_to_serial(term, '\n');
	}
	terminal_unlock(term);
}

/* Adds a character to the text buffer */
static int __terminal_buffer_char(struct terminal* term, char c)
{
	if(term->head >= term->textbuffer_size){
		if(__terminal_grow_textbuffer(term, term->head + 1) < 0){
			return -1;
		}
	}

	term->textbuffer[term->head++] = c;
	return 0;
}

static int __terminal_store_char(struct terminal* term, char c)
{
	if(term == NULL){
		return -1;
	}

	/* Before storing, so the log still gets the text when the text buffer can't grow */
	__terminal_copy_to_serial(term, c);

	return __terminal_buffer_char(term, c);
}

void terminal_commit()
{
	$process->current->term->ops->commit($process->current->term);
}

void terminal_putchar(char c)
{
	if($process->current == NULL || $process->current->term == NULL) return;
	
	/* putchar stores the character, which also copies it to the serial log */
	$process->current->term->ops->putchar($process->current->term, c);
}

/* terminal prototypes */
static int __terminal_write(struct terminal* term, const char* data, int size);
static int __terminal_writef(struct terminal* term, char* fmt, ...);
static int __terminal_putchar_graphics(struct terminal* term, char c);
static int __terminal_putchar_textmode(struct terminal* term, char c);
static int __terminal_commit_textmode(struct terminal* term);
static int __terminal_commit_graphics(struct terminal* term);
static int __terminal_attach(struct terminal* term);
static int __terminal_detach(struct terminal* term);
static int __terminal_reset(struct terminal* term);
static int __terminal_set_ops(struct terminal* term, struct terminal_ops* ops);
static int __terminal_scan_graphics(struct terminal* term, ubyte_t* data, int size);	
static int __terminal_scan_textmode(struct terminal* term, ubyte_t* data, int size);
static int __terminal_scanf(struct terminal* term, char* fmt, ...);
static unsigned char __terminal_getchar_graphics(struct terminal* term);
static unsigned char __terminal_getchar_textmode(struct terminal* term);

/* default terminal ops (graphics) */
struct terminal_ops terminal_ops = {
	.write = __terminal_write,
	.writef = __terminal_writef,
	.putchar = __terminal_putchar_graphics,
	.commit = __terminal_commit_graphics,
	.attach = __terminal_attach,
	.detach = __terminal_detach,
	.reset = __terminal_reset,
	.set = __terminal_set_ops,
	.scan = __terminal_scan_graphics,
	.scanf = __terminal_scanf,
	.getchar = __terminal_getchar_graphics,
};

/* text-mode terminal ops. Each table is shared by every terminal of its kind, so nothing writes to them */
struct terminal_ops terminal_ops_textmode = {
	.write = __terminal_write,
	.writef = __terminal_writef,
	.putchar = __terminal_putchar_textmode,
	.commit = __terminal_commit_textmode,
	.attach = __terminal_attach,
	.detach = __terminal_detach,
	.reset = __terminal_reset,
	.set = __terminal_set_ops,
	.scan = __terminal_scan_textmode,
	.scanf = __terminal_scanf,
	.getchar = __terminal_getchar_textmode,
};

/**
 * Creates a new terminal.
 * Attaches terminal ops to the terminal.
 * @return void
 */
struct terminal* terminal_create(terminal_flags_t flags)
{
	struct terminal* term = create(struct terminal);
	if(term == NULL) return NULL;

	term->textbuffer = (char*) kalloc(TERMINAL_TEXTBUFFER_INITIAL_CAPACITY);
	if(term->textbuffer == NULL){
		kfree(term);
		return NULL;
	}
	term->textbuffer_size = TERMINAL_TEXTBUFFER_INITIAL_CAPACITY;

	term->ops = HAS_FLAG(flags, TERMINAL_TEXT_MODE) ? &terminal_ops_textmode : &terminal_ops;

	term->head = 0;
	term->tail = 0;
	term->lines = 0;
	term->text_color =  0x1c;
	term->bg_color = COLOR_BLACK;
	term->screen = NULL;
	term->reader = TERMINAL_NO_READER;
	term->sel_anchor = -1;

	char* bg_color = kernel_config_get_value("terminal", "background");
	if(bg_color != NULL){
		term->bg_color = htoi(bg_color);
	}

	char* text_color = kernel_config_get_value("terminal", "text");
	if(text_color != NULL){
		term->text_color = htoi(text_color);
	}

	if(HAS_FLAG(flags, TERMINAL_TEXT_MODE)){
		term->bg_color = VGA_COLOR_BLUE;
		term->text_color = VGA_COLOR_WHITE;
	}

	/* The creator's reference, in place of the one it held on the terminal it had */
	term->refs = 1;
	struct terminal* old = $process->current->term;
	$process->current->term = term;
	terminal_put(old);

	return term;
}

/**
 * Takes a reference to a terminal. Every thread whose term points at a terminal holds one, so a terminal lives as
 * long as anything can write to it: a program started from a shell can outlive the shell (#55). Counted with
 * interrupts off rather than under a spinlock, so creating a kernel thread, which runs in a critical section, can
 * take one.
 */
void terminal_get(struct terminal* term)
{
	if(term == NULL) return;

	CRITICAL_SECTION({
		term->refs++;
	});
}

/* Releases a reference to a terminal, and frees it if that was the last */
void terminal_put(struct terminal* term)
{
	if(term == NULL) return;

	int refs;
	CRITICAL_SECTION({
		refs = --term->refs;
	});
	if(refs > 0) return;

	kfree(term->textbuffer);
	kfree(term->input);
	kfree(term->pending);
	kfree(term);
}

/**
 * Locks a terminal while its text changes or it's drawn. A shell's terminal is written by the shell's thread and by
 * any program it started, which prints from its own thread, and storing a character can move the text buffer.
 * The thread holding it can take it again, since the ops call each other (writef calls write, which calls putchar,
 * which may commit). Taken before a window's buffer_lock, never after. Not inside ENTER_CRITICAL, since spin_lock
 * waits by yielding.
 */
void terminal_lock(struct terminal* term)
{
	/* Only the holder sets the owner to itself, so this can't be another thread's lock */
	if(term->lock_owner == $process->current){
		term->lock_depth++;
		return;
	}

	spin_lock(&term->lock);
	term->lock_owner = $process->current;
	term->lock_depth = 1;
}

void terminal_unlock(struct terminal* term)
{
	if(--term->lock_depth > 0) return;

	term->lock_owner = NULL;
	spin_unlock(&term->lock);
}

/**
 * Called as pcb exits, before its window goes. The terminal stops drawing into the window, and if pcb was killed
 * while it held the terminal's lock, the lock is released. Once this has the lock, no commit is drawing into the
 * window.
 */
void terminal_release_window(struct terminal* term, struct pcb* pcb)
{
	if(term == NULL) return;

	if(term->lock_owner == pcb){
		/* A commit killed mid-draw leaves drawing set, which would keep the window server from copying its screen */
		if(term->screen != NULL) term->screen->drawing = false;
		term->lock_owner = NULL;
		term->lock_depth = 0;
		spin_unlock(&term->lock);
	}

	terminal_lock(term);
	if(term->screen != NULL && term->screen == pcb->gfx_window){
		term->screen->frames_atomic = false;
		term->screen = NULL;
	}
	terminal_unlock(term);
}

/* Terminal operations implementations */

static int __terminal_set_ops(struct terminal* term, struct terminal_ops* ops)
{
	if(term == NULL) return -1;

	/* overwrite ops that are not 0 */
	if(ops->write == 0) ops->write = term->ops->write;
	if(ops->writef == 0) ops->writef = term->ops->writef;
	if(ops->putchar == 0) ops->putchar = term->ops->putchar;
	if(ops->commit == 0) ops->commit = term->ops->commit;
	if(ops->attach == 0) ops->attach = term->ops->attach;
	if(ops->detach == 0) ops->detach = term->ops->detach;
	if(ops->reset == 0) ops->reset = term->ops->reset;
	if(ops->scan == 0) ops->scan = term->ops->scan;
	if(ops->scanf == 0) ops->scanf = term->ops->scanf;
	if(ops->getchar == 0) ops->getchar = term->ops->getchar;
	
	term->ops = ops;

	return 0;
}

static unsigned char __terminal_getchar_graphics(struct terminal* term)
{
	if(term == NULL) return -1;

	struct gfx_event event;
	gfx_event_loop(&event, GFX_EVENT_BLOCKING);

	switch (event.event){
	case GFX_EVENT_KEYBOARD:
		return event.data;
	case GFX_EVENT_EXIT:
		kernel_exit();
		return -1;
	default:
		break;
	}

	return -1;
}

static unsigned char __terminal_getchar_textmode(struct terminal* term)
{
	if(term == NULL) return -1;

	ubyte_t c = scr_keyboard_get(1);
	if(c == 0) return -1;

	return c;
}


/**
 * Reads a line typed at a terminal, as scan does for both modes: printable characters and backspace, ended by Enter.
 * The line joins the terminal's text, so it's shown and logged as what the reader wrote.
 * @return the line's length, with its '\n', followed by a 0 if there's room; 0 at end of input, Ctrl+D on an empty
 * line; or -1 for Ctrl+C, which throws the line away
 */
static int __terminal_scan_textmode(struct terminal* term, ubyte_t* data, int size)
{
	if(term == NULL || data == NULL || size < 1) return -1;

	/* Text mode draws only the text, so what's typed goes into it as it's typed */
	int length = 0;
	while (1){
		ubyte_t c = scr_keyboard_get(1);
		if(c == 0) continue;

		if(c == CTRLC){
			return -1;
		}
		if(c == CTRLD){
			if(length == 0) return 0;
			continue;
		}

		if(c == '\n'){
			data[length++] = '\n';
			if(length < size) data[length] = 0;
			term->ops->putchar(term, '\n');
			term->ops->commit(term);
			return length;
		}

		if(c == '\b'){
			if(length == 0) continue;
			length--;

			/* The last character typed is the text's last, and the log's */
			terminal_lock(term);
			term->head--;
			if(term->serial_line_length > 0) term->serial_line_length--;
			term->ops->commit(term);
			terminal_unlock(term);
			continue;
		}

		/* Only printable ASCII, with room left for the newline */
		if(c < 32 || c > 126 || length >= size - 1) continue;
		data[length++] = c;

		terminal_lock(term);
		term->ops->putchar(term, c);
		term->ops->commit(term);
		terminal_unlock(term);
	}
}

/* Ends a graphics terminal's read: the line being typed goes, and the window shows the text without it */
static void __terminal_end_read(struct terminal* term)
{
	terminal_hide_input(term);
	term->ops->commit(term);
}

/**
 * Reads a line typed at a graphics terminal, as __terminal_scan_textmode does. What's typed is edited on the input line,
 * after what the reader has written, and the keys come from the terminal's window: the reader's own, or for a program,
 * which has none, its shell's. The shell is waiting for the program meanwhile, so it doesn't take them. Ctrl+D or
 * closing the window is end of input; a window that's closing gets its event back, for its owner.
 */
static int __terminal_scan_graphics(struct terminal* term, ubyte_t* data, int size)
{
	if(term == NULL || data == NULL || size < 1) return -1;

	int length = 0;
	data[0] = 0;
	terminal_set_input(term, NULL, (char*)data);

	while (1){
		struct window* screen = term->screen;
		if(screen == NULL){
			terminal_hide_input(term);
			return 0;
		}

		/* A program killed while it reads exits when the read returns (pcb_kill) */
		if($process->current->kill_pending){
			__terminal_end_read(term);
			return -1;
		}

		/**
		 * Text pasted comes first, as if typed, so a newline in it ends the line, and the rest waits for the next read.
		 * The window isn't the reader's, so its events don't wake the reader: it looks every 10 ms.
		 */
		struct gfx_event event;
		int pasted = terminal_take_pending(term);
		if(pasted >= 0){
			event.event = GFX_EVENT_KEYBOARD;
			event.flags = 0;
			event.data = pasted;
		} else if(gfx_window_get_event(screen, &event) < 0){
			kernel_sleep(10);
			continue;
		}

		switch (event.event){
		case GFX_EVENT_KEYBOARD:{
				unsigned char c = event.data;
				if(terminal_key(term, c, event.flags)) break;

				if(c == CTRLC){
					terminal_drop_pending(term);
					__terminal_end_read(term);
					return -1;
				}
				if(c == CTRLD){
					if(length > 0) break;
					__terminal_end_read(term);
					return 0;
				}

				if(c == '\n'){
					data[length++] = '\n';
					if(length < size) data[length] = 0;

					terminal_hide_input(term);
					term->ops->write(term, (const char*)data, length);
					term->ops->commit(term);
					return length;
				}

				if(c == '\b'){
					if(length == 0) break;
					data[--length] = 0;
					terminal_set_input(term, NULL, (char*)data);
					break;
				}

				/* Only printable ASCII, with room left for the newline */
				if(c < 32 || c > 126 || length >= size - 1) break;
				data[length++] = c;
				data[length] = 0;
				terminal_set_input(term, NULL, (char*)data);
			}
			break;
		case GFX_EVENT_RESOLUTION:
			term->ops->commit(term);
			break;
		case GFX_EVENT_EXIT:
			gfx_push_event(screen, &event);
			__terminal_end_read(term);
			return 0;
		default:
			/* Selecting text works while a program reads, too */
			terminal_mouse(term, &event);
			break;
		}
	}
}

static int __terminal_scanf(struct terminal* term, char* fmt, ...)
{
	return -1;
}

static int __terminal_reset(struct terminal* term)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	term->head = 0;
	term->tail = 0;
	term->lines = 0;
	term->drawn_valid = false;
	term->sel_anchor = -1;
	term->selecting = false;

	memset(term->textbuffer, 0, term->textbuffer_size);
	terminal_unlock(term);

	return 0;
}

/**
 * The most lines a graphics terminal keeps: what a window the size of the screen shows, so a window that's made
 * bigger shows as much as fits, and drawing never takes more than a screenful. At least 25, for a screen whose size
 * isn't known.
 */
static int __terminal_max_lines(void)
{
	int lines = vbe_info->height / 8;
	return lines > 25 ? lines : 25;
}

/* And the most characters, for long lines: twice what a window the size of the screen shows, newlines included */
static int __terminal_max_chars(void)
{
	int columns = vbe_info->width / 8;
	return 2 * __terminal_max_lines() * ((columns > 80 ? columns : 80) + 1);
}

/* Drops text from the start of a graphics terminal's that no window could show, after c was added */
static void __terminal_trim(struct terminal* term, char c)
{
	if(c == '\n'){
		term->lines++;
		while(term->lines > __terminal_max_lines()){
			__terminal_remove_line(term);
			term->lines--;
		}
	}

	int max = __terminal_max_chars();
	if(term->head > max){
		/**
		 * The first line goes if what's left still fills a window the size of the screen, so it's above any window.
		 * Otherwise, as when a line is longer than that, the first half of the text goes, cutting a line.
		 */
		int first = term->lines > 0 ? __next_newline(term->textbuffer) : term->head;
		if(term->head - first >= max / 2){
			__terminal_remove_line(term);
			term->lines--;
		} else {
			int drop = term->head / 2;
			for (int i = 0; i < drop; i++){
				if(term->textbuffer[i] == '\n') term->lines--;
			}
			memmove(term->textbuffer, term->textbuffer + drop, term->head - drop);
			term->head -= drop;
			__terminal_selection_dropped(term, drop);
			/* The cut line wraps from a new start, so what's on screen moves */
			term->drawn_valid = false;
		}
	}
}

/* Stores a character in a graphics terminal */
static int __terminal_store_graphics(struct terminal* term, char c)
{
	if(__terminal_store_char(term, c) < 0){
		return -1;
	}

	__terminal_trim(term, c);
	return 0;
}

/* The columns and rows of text a window shows. A character is drawn at (1 + 8*column, 2 + 8*row) */
static int __terminal_columns(struct window* screen)
{
	int columns = (screen->inner_width - 1) / 8;
	return columns > 0 ? columns : 1;
}

static int __terminal_rows(struct window* screen)
{
	int rows = (screen->inner_height - 2) / 8;
	return rows > 0 ? rows : 1;
}

/* The rows a line of length characters takes, wrapped at columns: none when it's empty */
static int __terminal_wrapped_rows(int length, int columns)
{
	return (length + columns - 1) / columns;
}

/* Where the text's last line starts. It's unfinished: the text ends with a newline, or what's written next continues it */
static int __terminal_last_line(struct terminal* term)
{
	int start = term->head;
	while(start > 0 && term->textbuffer[start-1] != '\n') start--;
	return start;
}

/**
 * The rows the text's last line takes, with the input line, which continues it. With an input line it takes at least
 * a row, where typing appears; without, only rows for its characters.
 */
static int __terminal_last_rows(struct terminal* term, int columns)
{
	int length = term->head - __terminal_last_line(term);
	if(!term->input_shown){
		return __terminal_wrapped_rows(length, columns);
	}

	int rows = __terminal_wrapped_rows(length + term->input_length, columns);
	return rows > 0 ? rows : 1;
}

/**
 * Lays out a graphics terminal's text and input line from their end, wrapping lines at columns, so that the end fills
 * the last of rows rows. A finished line takes at least a row.
 * @param skip set to the rows of the first line to draw that are above the window
 * @return where in the text buffer to start drawing
 */
static int __terminal_first_shown(struct terminal* term, int columns, int rows, int* skip)
{
	char* text = term->textbuffer;
	int start = __terminal_last_line(term);

	int total = __terminal_last_rows(term, columns);
	while(total < rows && start > 0){
		/* The line before ends with the newline at start-1 */
		int end = start - 1;
		start = end;
		while(start > 0 && text[start-1] != '\n') start--;

		int line_rows = __terminal_wrapped_rows(end - start, columns);
		total += line_rows > 0 ? line_rows : 1;
	}

	*skip = total > rows ? total - rows : 0;
	return start;
}

/* Moves *x and *y, a column and row, past length characters of text, wrapping at columns, as drawing them would */
static void __terminal_advance(const char* text, int length, int columns, int* x, int* y)
{
	for (int i = 0; i < length; i++){
		if(text[i] == '\n'){
			*x = 0;
			(*y)++;
			continue;
		}

		if(*x == columns){
			*x = 0;
			(*y)++;
		}
		(*x)++;
	}
}

/* Clears count rows of text from row, the first from column x on. Rows outside the window's rows are left alone */
static void __terminal_clear_rows(struct terminal* term, struct window* screen, int x, int row, int count, int rows)
{
	for (int r = row; r < row + count; r++){
		if(r < 0 || r >= rows) continue;

		int left = r == row && x > 0 ? 1 + x*8 : 0;
		if(left < screen->inner_width){
			kernel_gfx_draw_rectangle(screen, left, 2 + r*8, screen->inner_width - left, 8, term->bg_color);
		}
	}
}

/* Moves the window's text up by count of its rows, and clears the rows that come free at the bottom */
static void __terminal_scroll_window(struct terminal* term, struct window* screen, int rows, int count)
{
	term->drawn_scrolled = true;
	term->drawn_y -= count;
	if(count > rows) count = rows;

	spin_lock(&screen->buffer_lock);
	/* Only if the window still has the size the rows were counted at: the window server may have just resized it,
	 * and then it sends an event that redraws it all */
	if(2 + rows*8 <= screen->inner_height){
		int bytes = screen->inner_width * (vbe_info->bpp/8);
		for (int y = 2; y < 2 + (rows - count)*8; y++){
			memcpy(screen->inner + y*screen->pitch, screen->inner + (y + count*8)*screen->pitch, bytes);
		}
	}
	spin_unlock(&screen->buffer_lock);

	__terminal_clear_rows(term, screen, 0, rows - count, count, rows);
}

/**
 * Draws length characters of text from column *x of row *y, wrapping at columns, and moves *x and *y past them. Rows
 * above the window aren't drawn. A row below the window's last, with a character on it or ended by a newline, even an
 * empty one, scrolls the window up first. index is where text starts in the text buffer, so selected characters are
 * drawn with the colors swapped, or -1 for the input line, which isn't selected.
 */
static void __terminal_draw_text(struct terminal* term, struct window* screen, const char* text, int index, int length, int columns, int rows, int* x, int* y)
{
	int sel_start = 0, sel_end = 0;
	bool_t selected = index >= 0 && __terminal_selection(term, &sel_start, &sel_end);

	for (int i = 0; i < length; i++){
		char c = text[i];
		if(c != '\n' && *x == columns){
			*x = 0;
			(*y)++;
		}
		if(*y >= rows){
			__terminal_scroll_window(term, screen, rows, *y - rows + 1);
			*y = rows - 1;
		}

		/* A selected newline is a cell at its line's end, if the row has room for one */
		bool_t inverse = selected && index + i >= sel_start && index + i < sel_end;
		if(inverse && *y >= 0 && *x < columns){
			kernel_gfx_draw_rectangle(screen, 1 + *x*8, 2 + *y*8, 8, 8, term->text_color);
		}

		if(c == '\n'){
			*x = 0;
			(*y)++;
			continue;
		}
		if(*y >= 0){
			kernel_gfx_draw_char(screen, 1 + *x*8, 2 + *y*8, c, inverse ? term->bg_color : __terminal_syntax_color(term, c));
		}
		(*x)++;
	}
}

/**
 * The last row with anything on it, for text that ends at column x of row y. A row the text has only just moved to is
 * empty, unless there's an input line, which takes a row even when it's empty: the row typing goes on.
 */
static int __terminal_last_used_row(struct terminal* term, int x, int y)
{
	return x == 0 && !term->input_shown ? y - 1 : y;
}

/* Draws the input line where the text ends, and notes the last row used */
static void __terminal_draw_input(struct terminal* term, struct window* screen, int columns, int rows)
{
	int x = term->drawn_x;
	int y = term->drawn_y;
	if(term->input_shown){
		if(y >= rows){
			__terminal_scroll_window(term, screen, rows, y - rows + 1);
			y = rows - 1;
		}
		__terminal_draw_text(term, screen, term->input, -1, term->input_length, columns, rows, &x, &y);
	}
	term->drawn_input_end = __terminal_last_used_row(term, x, y);
}

/* Draws the whole window: as much of the end of the text as fits, and the input line after it */
static void __terminal_draw_all(struct terminal* term, struct window* screen)
{
	/* Lay out the text and input line at the window's size now, so they fit however the window has changed */
	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);
	int skip;
	int start = __terminal_first_shown(term, columns, rows, &skip);

	kernel_gfx_draw_rectangle(screen, 0, 0, screen->inner_width, screen->inner_height, term->bg_color);

	/* Rows above the window, the first line's first skip rows, aren't drawn. The input line continues the text */
	int x = 0, y = -skip;
	__terminal_draw_text(term, screen, term->textbuffer + start, start, term->head - start, columns, rows, &x, &y);
	term->drawn_end = term->head;
	term->drawn_x = x;
	term->drawn_y = y;
	term->drawn_scrolled = start > 0 || skip > 0;
	__terminal_draw_input(term, screen, columns, rows);

	/* When the input line is longer than the window, the text's end is above it, and only a full draw will do */
	term->drawn_valid = term->drawn_y >= 0;
	term->drawn_width = screen->inner_width;
	term->drawn_height = screen->inner_height;
	term->drawn_text_color = term->text_color;
	term->drawn_bg_color = term->bg_color;
}

/**
 * Draws only what's changed since the window was last drawn: text added to the end, and the input line. The text added
 * goes where the input line was, scrolling the window when it reaches the bottom, and the input line follows it. So a
 * line written costs about the line, where a full draw costs the window (#118), and a key typed costs its row.
 * @return -1, having drawn nothing, when only a full draw will do: nothing's drawn yet, the window's size or the
 * colors changed, text was taken away rather than added, or the input line ends higher than it did once the text has
 * scrolled, which moves everything down
 */
static int __terminal_draw_changes(struct terminal* term, struct window* screen)
{
	if(!term->drawn_valid || term->drawn_width != screen->inner_width || term->drawn_height != screen->inner_height
		|| term->drawn_text_color != term->text_color || term->drawn_bg_color != term->bg_color){
		return -1;
	}
	if(term->drawn_end < 0 || term->drawn_end > term->head){
		return -1;
	}

	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);

	if(term->drawn_scrolled){
		int x = term->drawn_x, y = term->drawn_y;
		__terminal_advance(term->textbuffer + term->drawn_end, term->head - term->drawn_end, columns, &x, &y);
		if(term->input_shown){
			__terminal_advance(term->input, term->input_length, columns, &x, &y);
		}
		if(__terminal_last_used_row(term, x, y) < term->drawn_input_end){
			return -1;
		}
	}

	/* Take away the input line, add the text, and draw the input line after it */
	__terminal_clear_rows(term, screen, term->drawn_x, term->drawn_y, term->drawn_input_end - term->drawn_y + 1, rows);

	int x = term->drawn_x, y = term->drawn_y;
	__terminal_draw_text(term, screen, term->textbuffer + term->drawn_end, term->drawn_end, term->head - term->drawn_end, columns, rows, &x, &y);
	term->drawn_end = term->head;
	term->drawn_x = x;
	term->drawn_y = y;
	__terminal_draw_input(term, screen, columns, rows);

	return 0;
}

/**
 * The boundary between characters of the text at column col of row row, with the text laid out as __terminal_draw_all
 * lays it out: the window's first row is 0. Above the text shown is its start, and past a row's end is its end. Past
 * the text, as on the input line, is the text's end.
 */
static int __terminal_index_at_cell(struct terminal* term, struct window* screen, int col, int row)
{
	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);
	int skip;
	int start = __terminal_first_shown(term, columns, rows, &skip);

	if(col < 0) col = 0;
	if(col > columns) col = columns;

	int x = 0, y = -skip;
	for (int i = start; i < term->head; i++){
		char c = term->textbuffer[i];
		if(c != '\n' && x == columns){
			x = 0;
			y++;
		}

		/* Past the end of a full row is the start of the next */
		if(y > row || (y == row && x >= col)) return i;

		if(c == '\n'){
			if(y == row) return i;
			x = 0;
			y++;
			continue;
		}
		x++;
	}
	return term->head;
}

/* As __terminal_index_at_cell, for the point px, py in the window, nearest the edge of the character under it. Above the window is its first row */
static int __terminal_index_at(struct terminal* term, struct window* screen, int px, int py)
{
	int row = py < 2 ? 0 : (py - 2) / 8;
	int col = px < 1 ? 0 : (px - 1 + 4) / 8;
	return __terminal_index_at_cell(term, screen, col, row);
}

/* The column and row of the boundary before index in the text, laid out as __terminal_index_at_cell lays it out */
static void __terminal_cell_of(struct terminal* term, struct window* screen, int index, int* col, int* row)
{
	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);
	int skip;
	int start = __terminal_first_shown(term, columns, rows, &skip);

	int x = 0, y = -skip;
	for (int i = start; i < index && i < term->head; i++){
		char c = term->textbuffer[i];
		if(c != '\n' && x == columns){
			x = 0;
			y++;
		}
		if(c == '\n'){
			x = 0;
			y++;
			continue;
		}
		x++;
	}
	*col = x;
	*row = y;
}

/**
 * Adds the clipboard's text to what's pending, for the shell or a program reading a line to take as if it was typed.
 * Tabs become spaces, and other control characters, all but newlines, are left out, as typing them does nothing.
 */
static void __terminal_paste(struct terminal* term)
{
	int size = clipboard_get(NULL, 0);
	if(size <= 0) return;

	char* text = kalloc_try(size);
	if(text == NULL) return;
	int got = clipboard_get(text, size);
	if(got < size) size = got > 0 ? got : 0;

	/* What's still pending, then the new text */
	int left = term->pending_length - term->pending_position;
	char* pending = kalloc_try(left + size);
	if(pending == NULL){
		kfree(text);
		return;
	}
	if(left > 0) memcpy(pending, term->pending + term->pending_position, left);

	int length = left;
	for (int i = 0; i < size; i++){
		char c = text[i] == '\t' ? ' ' : text[i];
		if(c != '\n' && (c < 32 || c > 126)) continue;
		pending[length++] = c;
	}
	kfree(text);

	kfree(term->pending);
	term->pending = pending;
	term->pending_length = length;
	term->pending_position = 0;
}

/**
 * Takes the next character pasted into the terminal and not yet taken: the shell and a program reading a line take
 * them before new keys, as if they were typed.
 * @return the character, or -1 if there's none
 */
int terminal_take_pending(struct terminal* term)
{
	if(term == NULL) return -1;

	int c = -1;
	terminal_lock(term);
	if(term->pending_position < term->pending_length){
		c = (unsigned char) term->pending[term->pending_position++];
	}
	if(term->pending != NULL && term->pending_position >= term->pending_length){
		kfree(term->pending);
		term->pending = NULL;
		term->pending_length = 0;
		term->pending_position = 0;
	}
	terminal_unlock(term);
	return c;
}

/* Throws away what was pasted and not yet taken, as Ctrl+C does with what's being typed */
void terminal_drop_pending(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	kfree(term->pending);
	term->pending = NULL;
	term->pending_length = 0;
	term->pending_position = 0;
	terminal_unlock(term);
}

/**
 * Handles the keys that select, copy and paste a graphics terminal's text, before its shell or a program reading a line
 * sees them:
 * - Shift with an arrow moves the selection's end, by a character or a row on screen, starting a selection at the end
 *   of the text if there's none. Ctrl+Shift+A selects all the text, and Esc clears the selection.
 * - Ctrl+Shift+C copies the selection to the clipboard. So does Ctrl+C when something is selected, which also clears
 *   it; with nothing selected, Ctrl+C is left to cancel the line.
 * - Ctrl+V and Ctrl+Shift+V paste: the clipboard's text is added to what's pending (terminal_take_pending).
 * @return 1 if it handled key, 0 to leave it to the caller, as Esc or Ctrl+C with nothing selected
 */
int terminal_key(struct terminal* term, unsigned char key, unsigned char modifiers)
{
	if(term == NULL || term->ops->commit != __terminal_commit_graphics) return 0;

	bool_t shift = (modifiers & KEY_MOD_SHIFT) != 0;
	bool_t arrow = key == ARROW_LEFT || key == ARROW_RIGHT || key == ARROW_UP || key == ARROW_DOWN;
	if(!(shift && (arrow || key == CTRLA)) && key != 27 && key != CTRLC && key != CTRLV) return 0;

	terminal_lock(term);
	struct window* screen = term->screen;
	int start, end;
	bool_t selected = __terminal_selection(term, &start, &end);
	if(screen == NULL || (key == 27 && term->sel_anchor < 0) || (key == CTRLC && !shift && !selected)){
		terminal_unlock(term);
		return 0;
	}

	if(key == CTRLV){
		__terminal_paste(term);
		terminal_unlock(term);
		return 1;
	}

	if(key == CTRLC){
		if(selected){
			clipboard_set(term->textbuffer + start, end - start);
		}
		/* Ctrl+Shift+C leaves the selection, as most terminals do. Ctrl+C clears it, so the next one cancels again */
		if(shift){
			terminal_unlock(term);
			return 1;
		}
		term->sel_anchor = -1;
	} else if(key == 27){
		term->sel_anchor = -1;
	} else if(key == CTRLA){
		term->sel_anchor = 0;
		term->sel_end = term->head;
	} else {
		if(term->sel_anchor < 0){
			term->sel_anchor = term->head;
			term->sel_end = term->head;
		}

		int moving = term->sel_end < term->head ? term->sel_end : term->head;
		if(key == ARROW_LEFT){
			moving = moving > 0 ? moving - 1 : 0;
		} else if(key == ARROW_RIGHT){
			moving = moving < term->head ? moving + 1 : term->head;
		} else {
			int col, row;
			__terminal_cell_of(term, screen, moving, &col, &row);
			moving = __terminal_index_at_cell(term, screen, col, key == ARROW_UP ? row - 1 : row + 1);
		}
		term->sel_end = moving;
	}
	term->selecting = false;

	term->drawn_valid = false;
	term->ops->commit(term);
	terminal_unlock(term);
	return 1;
}

/* Whether c is part of a word, for a double click: a letter, a digit or _ */
static bool_t __terminal_word_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/**
 * Selects what's around the character at index in the text, for a double click: its word, a run of letters, digits and
 * _, or else a run of the same character, like spaces. Past a line's end is its last character. With line, for a
 * triple click, selects its whole line and the newline after it.
 */
static void __terminal_select_around(struct terminal* term, int index, bool_t line)
{
	char* text = term->textbuffer;
	int i = index < term->head ? index : term->head;
	if(i < 0) i = 0;

	if(line){
		int start = i;
		while(start > 0 && text[start-1] != '\n') start--;
		int end = i;
		while(end < term->head && text[end] != '\n') end++;
		term->sel_anchor = start;
		term->sel_end = end < term->head ? end + 1 : end;
		return;
	}

	if((i >= term->head || text[i] == '\n') && i > 0 && text[i-1] != '\n') i--;
	if(i >= term->head || text[i] == '\n'){
		/* An empty line has nothing to select */
		term->sel_anchor = -1;
		return;
	}

	char c = text[i];
	bool_t word = __terminal_word_char(c);
	int start = i, end = i + 1;
	while(start > 0 && (word ? __terminal_word_char(text[start-1]) : text[start-1] == c)) start--;
	while(end < term->head && (word ? __terminal_word_char(text[end]) : text[end] == c)) end++;
	term->sel_anchor = start;
	term->sel_end = end;
}

/**
 * Selects a graphics terminal's text with the mouse: a press starts a selection where it is, a drag moves its end, and
 * a release without a drag clears it. A press with Shift moves the end of the selection there is. A double click
 * selects a word, and a triple click a line, without a drag. It's called by
 * whoever is reading the terminal's window's events: its shell, or a program reading a line. The input line isn't
 * selected, so a drag over it selects to the text's end.
 * @return 1 if event was a press, drag or release, which it handled, 0 if not
 */
int terminal_mouse(struct terminal* term, struct gfx_event* event)
{
	if(term == NULL || event == NULL) return 0;
	if(event->event != GFX_EVENT_MOUSE_DOWN && event->event != GFX_EVENT_MOUSE_MOVE && event->event != GFX_EVENT_MOUSE_UP){
		return 0;
	}

	terminal_lock(term);
	struct window* screen = term->screen;
	if(screen == NULL || term->ops->commit != __terminal_commit_graphics){
		terminal_unlock(term);
		return 1;
	}

	int px = (int16_t)event->data, py = (int16_t)event->data2;
	int index = __terminal_index_at(term, screen, px, py);
	if(event->event == GFX_EVENT_MOUSE_DOWN && GFX_EVENT_CLICKS(event->flags) >= 2){
		/* The character under the mouse, rather than the nearer edge of one */
		__terminal_select_around(term, __terminal_index_at(term, screen, px - 4, py), GFX_EVENT_CLICKS(event->flags) == 3);
		term->selecting = false;
	} else if(event->event == GFX_EVENT_MOUSE_DOWN){
		if(!(event->flags & KEY_MOD_SHIFT) || term->sel_anchor < 0){
			term->sel_anchor = index;
		}
		term->sel_end = index;
		term->selecting = true;
	} else if(term->selecting){
		term->sel_end = index;
		if(event->event == GFX_EVENT_MOUSE_UP){
			term->selecting = false;
			if(term->sel_end == term->sel_anchor) term->sel_anchor = -1;
		}
	} else {
		terminal_unlock(term);
		return 1;
	}

	/* What's selected changes the colors of text already drawn */
	term->drawn_valid = false;
	term->ops->commit(term);
	terminal_unlock(term);
	return 1;
}

static int __terminal_putchar_graphics(struct terminal* term, char c)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	if(__terminal_store_graphics(term, c) < 0){
		terminal_unlock(term);
		return -1;
	}

	/* should be a flush syscall */
	if(c == '\n' && term->screen != NULL && term->screen != $process->current->gfx_window){
		term->ops->commit(term);
	}

	if(term->screen != NULL && term->screen != $process->current->gfx_window){
		term->screen->changed = 1;
		gfx_composition_wake();
	} else {
		gfx_commit();
	}
	terminal_unlock(term);

	return 1;
}

/**
 * Writes text to the calling thread's terminal: the system call programs print with. putchar redraws another
 * thread's graphics terminal at every newline, so this stores all of the text first and redraws once.
 * Without a terminal, the text is dropped, as terminal_putchar does.
 * @return size, or a negative error code
 */
int terminal_write(const char* data, int size)
{
	if(data == NULL || size < 0) return -ERROR_INVALID_ARGUMENTS;
	if($process->current == NULL || $process->current->term == NULL) return size;

	struct terminal* term = $process->current->term;

	/* One write's text stays together, whoever else writes to the terminal */
	terminal_lock(term);
	if(term->ops->putchar != __terminal_putchar_graphics || term->screen == NULL){
		for (int i = 0; i < size; i++){
			term->ops->putchar(term, data[i]);
		}
		terminal_unlock(term);
		return size;
	}

	for (int i = 0; i < size; i++){
		if(__terminal_store_graphics(term, data[i]) < 0) break;
	}

	/* As putchar does: another thread's terminal is redrawn now, and a thread's own when it commits */
	if(term->screen != $process->current->gfx_window){
		term->ops->commit(term);
	} else {
		gfx_commit();
	}
	terminal_unlock(term);
	return size;
}

/**
 * Reads a line typed at the calling program's terminal: the system call behind its standard input.
 * @return the line's length, ending in '\n' unless size was too small; 0 at end of input; or a negative error code.
 * End of input is Ctrl+D on an empty line, Ctrl+C, which throws the line away, since RetrOS has no signals, and the
 * window closing. A program its shell isn't waiting for, like one started with exec -d, always gets it: the shell
 * is taking the keys.
 */
int terminal_read(char* data, int size)
{
	if(data == NULL || size < 0) return -ERROR_INVALID_ARGUMENTS;
	if(size == 0 || $process->current == NULL || $process->current->term == NULL) return 0;

	struct terminal* term = $process->current->term;

	/* While the shell is starting a program, which may be this one, the reader isn't known yet */
	while(term->reader == TERMINAL_READER_STARTING){
		kernel_yield();
	}
	if(term->reader != $process->current->pid) return 0;

	int ret = term->ops->scan(term, (ubyte_t*)data, size);
	return ret < 0 ? 0 : ret;
}

/* Makes room for a new line of text in a text mode terminal, scrolling its text up a line when its frame is full */
static void __terminal_new_line_textmode(struct terminal* term)
{
	if(SCREEN_HEIGHT-2 <= term->lines){
		__terminal_scroll(term);
	} else {
		term->lines++;
	}
}

static int __terminal_putchar_textmode(struct terminal* term, char c)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	if(c == '\n'){
		__terminal_new_line_textmode(term);
	}

	int ret = __terminal_store_char(term, c);
	terminal_unlock(term);

	return ret < 0 ? -1 : 1;
}


static int __terminal_write(struct terminal* term, const char* data, int size)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	for (int i = 0; i < size; i++){
		term->ops->putchar(term, data[i]);
	}
	terminal_unlock(term);

	return size;
}

static int __terminal_attach(struct terminal* term)
{
	if(term == NULL) return -1;

	/* The thread swaps its reference to the terminal it had for one to this */
	struct terminal* old = $process->current->term;
	if(old != term){
		terminal_get(term);
		$process->current->term = term;
		terminal_put(old);
	}

	if(term->screen == NULL){
		term->screen = $process->current->gfx_window;
		if(term->screen != NULL){
			/* Its commits draw a whole frame, so the window server can copy it once changed is set (#165) */
			term->screen->frames_atomic = true;
		}
	}

	return 0;
}

static int __terminal_detach(struct terminal* term)
{
	if(term == NULL) return -1;

	$process->current->term = NULL;
	if(term->screen != NULL){
		term->screen->frames_atomic = false;
		term->screen = NULL;
	}

	terminal_put(term);

	return 0;
}

static int __terminal_commit_textmode(struct terminal* term)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	/* Clear screen */
	for (int i = 1; i < SCREEN_HEIGHT-1; i++){
		for (int j = 1; j < SCREEN_WIDTH-2; j++){
			scrput(j, i, ' ', term->text_color | term->bg_color << 4);
		}
	}

	int x = 0, y = 1;
	for (int i = term->tail; i < term->head; i++){
		if(term->textbuffer[i] == '\n'){
			x = 0;
			y++;
			continue;
		}

		scrput(1+x, y, term->textbuffer[i], term->text_color | term->bg_color << 4);
		x++;
	}
	terminal_unlock(term);

	return 0;
}

static int __terminal_commit_graphics(struct terminal* term)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	struct window* screen = term->screen;
	if(screen == NULL){
		terminal_unlock(term);
		return -1;
	}

	/**
	 * Clear changed and mark the frame as drawing, so the window server doesn't copy a half-drawn frame: it copies
	 * once changed is set again, after the draw. A fullscreen window draws straight into the composition buffer, so
	 * a program selecting text while its shell is blocked would otherwise flicker (#165).
	 */
	screen->changed = 0;
	screen->drawing = true;

	if(__terminal_draw_changes(term, screen) < 0){
		__terminal_draw_all(term, screen);
	}
	screen->changed = 1;
	screen->drawing = false;
	gfx_composition_wake();
	terminal_unlock(term);

	return 0;
}

/**
 * Shows prompt and text, either of which may be NULL, on the terminal's input line, and redraws it. The input line is
 * what a shell, or a program reading a line, is editing. It's drawn after the text and wrapped like it, continuing
 * the text's last line, but it isn't part of the text: it's never copied to the log, and text written meanwhile goes
 * before it, as readline does. Text mode doesn't show it.
 */
void terminal_set_input(struct terminal* term, const char* prompt, const char* text)
{
	if(term == NULL) return;

	int prompt_length = prompt != NULL ? strlen(prompt) : 0;
	int text_length = text != NULL ? strlen(text) : 0;
	int length = prompt_length + text_length;

	terminal_lock(term);
	if(length > term->input_size){
		int size = term->input_size > 0 ? term->input_size : 128;
		while(size < length){
			size *= 2;
		}

		char* input = (char*) krealloc(term->input, term->input_size, size);
		if(input == NULL){
			warningf("[TERM] Failed to grow input line to %d bytes\n", size);
			terminal_unlock(term);
			return;
		}
		term->input = input;
		term->input_size = size;
	}

	memcpy(term->input, prompt, prompt_length);
	memcpy(term->input + prompt_length, text, text_length);
	term->input_length = length;
	term->input_shown = true;

	if(term->ops->commit == __terminal_commit_graphics){
		__terminal_commit_graphics(term);
	}
	terminal_unlock(term);
}

/* Makes the terminal's next redraw a full one: after something else may have drawn into its window */
void terminal_invalidate(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	term->drawn_valid = false;
	terminal_unlock(term);
}

/* Removes the terminal's input line. The next redraw shows the text without it */
void terminal_hide_input(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	term->input_shown = false;
	term->input_length = 0;
	terminal_unlock(term);
}

/**
 * Ends a terminal's unfinished last line, if it has one, so what's written next starts a row of its own: a shell's
 * prompt after a program's printf("done"), or after cat of a file that doesn't end its line. The log already has that
 * line, under the program's name, if the program has exited, so the newline isn't logged again then.
 */
void terminal_end_line(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	if(term->head > 0 && term->textbuffer[term->head-1] != '\n'){
		if(term->serial_line_length > 0){
			__terminal_copy_to_serial(term, '\n');
		}
		if(term->ops->putchar == __terminal_putchar_textmode){
			__terminal_new_line_textmode(term);
			__terminal_buffer_char(term, '\n');
		} else if(__terminal_buffer_char(term, '\n') == 0){
			__terminal_trim(term, '\n');
		}
	}
	terminal_unlock(term);
}

static int __terminal_writef(struct terminal* term, char* fmt, ...)
{
	if(term == NULL) return -1;

	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[MAX_FMT_STR_SIZE];
	int num = 0;
	int padding = 0;

	va_start(args, fmt);

	/* One call's text stays together, whoever else writes to the terminal */
	terminal_lock(term);
	while (*fmt != '\0') {
		switch (*fmt){
			case '%':
				memset(str, 0, MAX_FMT_STR_SIZE);
				
				/* Check if the format specifier is a digit (for padding) */
				padding = 0;
				if (*(fmt+1) >= '0' && *(fmt+1) <= '9') {
					padding = *(fmt+1) - '0';
					fmt++;
				}

				switch (*(fmt+1))
				{
					case 'd': ;
						num = va_arg(args, int);
						itoa(num, str);
						term->ops->write(term, str, strlen(str));
						x_offset += strlen(str);
						break;
					case 'i': ;
						num = va_arg(args, int);
						unsigned char bytes[4];
						bytes[0] = (num >> 24) & 0xFF;
						bytes[1] = (num >> 16) & 0xFF;
						bytes[2] = (num >> 8) & 0xFF;
						bytes[3] = num & 0xFF;
						term->ops->writef(term, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
						break;
					case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);

						if(strlen(str) < 5){
							int pad = 5-strlen(str);
							for (int i = 0; i < pad; i++){
								term->ops->putchar(term, '0');
							}
						}

						term->ops->write(term, str, strlen(str));
						x_offset += strlen(str);
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						term->ops->write(term, str, strlen(str));
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						if (str_arg == NULL) str_arg = "(null)";
						int len = strlen(str_arg);
						term->ops->write(term, str_arg, len);
						x_offset += strlen(str_arg);

						/* Pad the string if needed */
						if (padding > len) {
							for (int i = 0; i < padding - len; i++) {
								term->ops->putchar(term, ' ');
								x_offset++;
							}
						}

						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						term->ops->putchar(term, char_arg);
						x_offset++;
						break;
					default:
						term->ops->putchar(term, *fmt);
						x_offset++;
						break;
				}
				fmt++;
				break;
			default:  
				term->ops->putchar(term, *fmt);
				x_offset++;
			}
        fmt++;
    }
	terminal_unlock(term);

	va_end(args);
	written += x_offset;
	return written;
}
