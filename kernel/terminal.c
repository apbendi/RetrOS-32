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

static void __terminal_remove_line(struct terminal* term)
{
	int skip = __next_newline(term->textbuffer);
	memmove(term->textbuffer, &term->textbuffer[skip], term->head-skip);
	term->head -= skip;
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

	char* new_buffer = (char*) krealloc(term->textbuffer, new_size);
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
		term->serial_line[term->serial_line_length++] = c;
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
	term->text_version++;
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

	/* Nothing's drawn yet, so the first redraw is a full one */
	term->drawn_version = term->text_version - 1;

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
		term->lock_owner = NULL;
		term->lock_depth = 0;
		spin_unlock(&term->lock);
	}

	terminal_lock(term);
	if(term->screen != NULL && term->screen == pcb->gfx_window){
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


static int __terminal_scan_textmode(struct terminal* term, ubyte_t* data, int size)
{
	if(term == NULL || data == NULL) return -1;

	int i = 0;
	ubyte_t c = 0;
	while (i < size && c != '\n'){
		c = scr_keyboard_get(1);
		if(c == 0) continue;

		if(c == CTRLC){
			return -1;
		}

		if(c == '\n') {
			break;
		}

		if(c == '\b'){
			data[i] = 0;
			i--;
			if(i < 0) i = 0;
			terminal_lock(term);
			term->head--;
			term->textbuffer[term->head] = 0;
			term->ops->commit(term);
			terminal_unlock(term);
			continue;
		}

		data[i] = c;
		i++;

		terminal_lock(term);
		term->ops->putchar(term, c);
		term->ops->commit(term);
		terminal_unlock(term);
	}

	data[i] = '\0';

	return i;
}

static int __terminal_scan_graphics(struct terminal* term, ubyte_t* data, int size)
{
	if(term == NULL || data == NULL) return -1;

	int i = 0;
	ubyte_t c = 0;
	while (i < size && c != '\n'){
		struct gfx_event event;
		gfx_event_loop(&event, GFX_EVENT_BLOCKING);

		switch (event.event){
		case GFX_EVENT_KEYBOARD:
			switch (event.data){
			case CTRLC:
				return -1;
			default:{
					c = event.data;

					if(c == '\b'){
						data[i] = 0;
						i--;
						if(i < 0) i = 0;
						terminal_lock(term);
						term->head--;
						term->textbuffer[term->head] = 0;
						term->ops->commit(term);
						terminal_unlock(term);
						break;
					}

					data[i] = c;
					i++;

					terminal_lock(term);
					term->ops->putchar(term, c);
					term->ops->commit(term);
					terminal_unlock(term);
				}
				break;
			}
			break;
		case GFX_EVENT_EXIT:
			kernel_exit();
			return -1;
		default:
			break;
		}
	}

	data[i] = '\0';

	return i;
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
	term->text_version++;

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
 * @param shown set to the rows drawn: rows, or fewer when everything fits
 * @return where in the text buffer to start drawing
 */
static int __terminal_first_shown(struct terminal* term, int columns, int rows, int* skip, int* shown)
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
	*shown = total > rows ? rows : total;
	return start;
}

/* Draws length characters of text from column *x of row *y, wrapping at columns, and moves *x and *y past them. Rows
 * above the window aren't drawn */
static void __terminal_draw_text(struct terminal* term, struct window* screen, const char* text, int length, int columns, int* x, int* y)
{
	for (int i = 0; i < length; i++){
		char c = text[i];
		if(c == '\n'){
			*x = 0;
			(*y)++;
			continue;
		}

		if(*x == columns){
			*x = 0;
			(*y)++;
		}
		if(*y >= 0){
			kernel_gfx_draw_char(screen, 1 + *x*8, 2 + *y*8, c, __terminal_syntax_color(term, c));
		}
		(*x)++;
	}
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

static int __terminal_putchar_textmode(struct terminal* term, char c)
{
	if(term == NULL) return -1;

	terminal_lock(term);
	if(c == '\n'){
		if(SCREEN_HEIGHT-2 <= term->lines){
			__terminal_scroll(term);
		} else {
			term->lines++;
		}
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
	}

	return 0;
}

static int __terminal_detach(struct terminal* term)
{
	if(term == NULL) return -1;

	$process->current->term = NULL;
	term->screen = NULL;

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

	/* Lay out the text and input line at the window's size now, so they fit however the window has changed */
	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);
	int skip, shown;
	int start = __terminal_first_shown(term, columns, rows, &skip, &shown);

	kernel_gfx_draw_rectangle(screen, 0, 0, screen->inner_width, screen->inner_height, term->bg_color);

	/* Rows above the window, the first line's first skip rows, aren't drawn. The input line continues the text */
	int x = 0, y = -skip;
	__terminal_draw_text(term, screen, term->textbuffer + start, term->head - start, columns, &x, &y);
	if(term->input_shown){
		__terminal_draw_text(term, screen, term->input, term->input_length, columns, &x, &y);
	}

	term->drawn_version = term->text_version;
	term->drawn_width = screen->inner_width;
	term->drawn_height = screen->inner_height;
	term->drawn_last_rows = __terminal_last_rows(term, columns);
	screen->changed = 1;
	terminal_unlock(term);

	return 0;
}

/**
 * Redraws a graphics terminal after a change to its input line alone. When the text, the window and the rows the last
 * line takes are as they were at the last full redraw, only the last line's rows change, so only they're drawn: a key
 * typed in a full window costs a row, not the window. Otherwise it's a full redraw.
 */
static void __terminal_redraw_input(struct terminal* term)
{
	struct window* screen = term->screen;
	if(screen == NULL) return;

	int columns = __terminal_columns(screen);
	int rows = __terminal_rows(screen);
	int last_rows = __terminal_last_rows(term, columns);
	if(term->drawn_version != term->text_version || term->drawn_width != screen->inner_width
		|| term->drawn_height != screen->inner_height || term->drawn_last_rows != last_rows || last_rows > rows){
		__terminal_commit_graphics(term);
		return;
	}

	/* The last line is at the bottom of what's drawn */
	int skip, shown;
	__terminal_first_shown(term, columns, rows, &skip, &shown);
	int x = 0, y = shown - last_rows;
	kernel_gfx_draw_rectangle(screen, 0, 2 + y*8, screen->inner_width, last_rows*8, term->bg_color);

	int line = __terminal_last_line(term);
	__terminal_draw_text(term, screen, term->textbuffer + line, term->head - line, columns, &x, &y);
	__terminal_draw_text(term, screen, term->input, term->input_length, columns, &x, &y);
	screen->changed = 1;
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

		char* input = (char*) krealloc(term->input, size);
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
		__terminal_redraw_input(term);
	}
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
 * Ends a graphics terminal's unfinished last line, if it has one, so what's written next starts a row of its own: a
 * shell's prompt after a program's printf("done"). The log already has that line, under the program's name, if the
 * program has exited, so the newline isn't logged again then.
 */
void terminal_end_line(struct terminal* term)
{
	if(term == NULL) return;

	terminal_lock(term);
	if(term->head > 0 && term->textbuffer[term->head-1] != '\n'){
		if(term->serial_line_length > 0){
			__terminal_copy_to_serial(term, '\n');
		}
		if(__terminal_buffer_char(term, '\n') == 0){
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
