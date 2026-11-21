#ifndef TERMINAL_H
#define TERMINAL_H

#include <pcb.h>
#include <kutils.h>
#include <colors.h>
#include <gfx/window.h>
#include <libc.h>
#include <lib/scrollbar.h>

#define TERM_CONTEXT(codeblock) do { \
    struct terminal* term = $process->current->term; \
    if(term == NULL) { \
        break;\
    } \
    codeblock \
} while(0)

#define twritef(a, ...) \
 if($process->current != NULL && $process->current->term != NULL) { \
    $process->current->term->ops->writef($process->current->term, a, ##__VA_ARGS__); \
 }

typedef enum {
    TERMINAL_FLAG_NONE = 0 << 0,
    TERMINAL_TEXT_MODE = 1 << 0,
    TERMINAL_GRAPHICS_MODE = 1 << 1,
} terminal_flags_t;

struct terminal;
struct terminal_ops {
    int (*write)(struct terminal* term, const char* data, int size);
    int (*writef)(struct terminal* term, char* fmt, ...);
    int (*putchar)(struct terminal* term, char c);
    int (*commit)(struct terminal* term);
    int (*attach)(struct terminal* term);
    int (*set)(struct terminal* term, struct terminal_ops* ops);
    int (*detach)(struct terminal* term);
    int (*reset)(struct terminal* term);

    /**
     * @brief Read data from the terminal (keyboard)
     * @param term Terminal to read from
     * @param data Buffer to read into
     * @param size Size of the buffer
     * 
     * @return int Amount of bytes read
     */
    int (*scan)(struct terminal* term, ubyte_t* data, int size);
    int (*scanf)(struct terminal* term, char* fmt, ...);
    unsigned char (*getchar)(struct terminal* term);
};

/* A terminal's reader when there's none, and while its shell is starting a program that may become it */
#define TERMINAL_NO_READER -1
#define TERMINAL_READER_STARTING -2

/* Longest line copied to the serial log in one piece; longer lines are split */
#define TERMINAL_SERIAL_LINE_SIZE 128

/* How often a selection dragged above or below a Terminal's rows scrolls it a row, as the Editor's does */
#define TERMINAL_DRAG_SCROLL_MS 50

struct terminal {
    char* textbuffer;
    int textbuffer_size;

    /* The line being written, copied to the serial log when it ends */
    char serial_line[TERMINAL_SERIAL_LINE_SIZE];
    int serial_line_length;

    int tail;
    int head;
    int lines;

    /* References held, one per thread whose term points here: see terminal_get */
    int refs;

    /* Held while the terminal's text changes or it's drawn: see terminal_lock */
    spinlock_t lock;
    struct pcb* lock_owner;
    int lock_depth;

    /* The program that may read lines typed at the terminal: see terminal_read */
    int reader;

    /**
     * The line being typed, drawn after the text: see terminal_set_input. input_shown is false when there's none.
     * input_cursor is where typing goes, as an index into input, from 0 to input_length.
     */
    char* input;
    int input_size;
    int input_length;
    int input_cursor;
    bool_t input_shown;

    /**
     * What's on screen, so a redraw draws only what's changed since (#118). The text is drawn up to drawn_end in the
     * text buffer, and what's written next goes at column drawn_x of row drawn_y. drawn_input_end is the last row used,
     * by the input line after the text or by the text. drawn_scrolled is set once the text's start is above the window.
     */
    bool_t drawn_valid;
    int drawn_end;
    int drawn_x;
    int drawn_y;
    int drawn_input_end;
    bool_t drawn_scrolled;
    int drawn_width;
    int drawn_height;
    color_t drawn_text_color;
    color_t drawn_bg_color;
    /* The view drawn, as view_top, and whether the scroll bar is on screen */
    int drawn_view_top;
    bool_t drawn_bar;

    /**
     * Scrolling back (#147): view_top is where in the text buffer the top row shown starts, or -1 while the view follows
     * the text's end. view_row is that row's number, counting the text's rows from its start. A view that reaches the
     * last row follows the end instead, so new text doesn't move a view scrolled back, and does move one that isn't.
     */
    int view_top;
    int view_row;

    /**
     * The text's rows, counted ahead so printing costs only what's printed: rows_lines rows for the finished lines before
     * rows_done, which is just past the last newline counted, laid out at rows_columns columns, or 0 for none counted.
     * See __terminal_count_rows.
     */
    int rows_columns;
    int rows_done;
    int rows_lines;

    /* The scroll bar at the window's right edge, drawn while the text doesn't all fit */
    struct scrollbar bar;

    /**
     * While the mouse is selecting above (-1) or below (1) the rows shown, the view scrolls a row every
     * TERMINAL_DRAG_SCROLL_MS (terminal_drag_tick), selecting to the row at drag_x that comes into view. 0 otherwise.
     */
    int drag_scroll;
    int drag_x;

    /**
     * The selection: the text between sel_anchor, where it started, and sel_end, the end that moves, as indexes into the
     * text buffer. sel_anchor is -1 for none. selecting is set while the mouse is dragging it. See terminal_mouse.
     */
    int sel_anchor;
    int sel_end;
    bool_t selecting;

    /* Text pasted, which the shell or a program reading a line takes before new keys, as if typed: see terminal_key */
    char* pending;
    int pending_length;
    int pending_position;

    struct window* screen;
    struct terminal_ops* ops;
    color_t text_color;
    color_t bg_color;
};

/**
 * A line being typed: the shell's command line, or a line a program reads. buffer holds size bytes: up to size-1
 * characters, then a 0. cursor is where typing goes, from 0 to length. See terminal_line_key.
 */
struct terminal_line {
    char* buffer;
    int size;
    int length;
    int cursor;
};

void terminal_line_init(struct terminal_line* line, char* buffer, int size);
void terminal_line_set(struct terminal_line* line, const char* text);
bool_t terminal_line_insert(struct terminal_line* line, char c);
bool_t terminal_line_key(struct terminal_line* line, unsigned char key);

struct terminal* terminal_create(terminal_flags_t flags);
void terminal_get(struct terminal* term);
void terminal_put(struct terminal* term);
void terminal_lock(struct terminal* term);
void terminal_unlock(struct terminal* term);
void terminal_release_window(struct terminal* term, struct pcb* pcb);
void terminal_set_input(struct terminal* term, const char* prompt, const char* text, int cursor);
void terminal_hide_input(struct terminal* term);
void terminal_end_line(struct terminal* term);
void terminal_invalidate(struct terminal* term);
int terminal_mouse(struct terminal* term, struct gfx_event* event);
int terminal_key(struct terminal* term, unsigned char key, unsigned char modifiers);
bool_t terminal_drag_scrolling(struct terminal* term);
void terminal_drag_tick(struct terminal* term);
int terminal_take_pending(struct terminal* term);
void terminal_drop_pending(struct terminal* term);

int scan(ubyte_t* data, int size);

void twrite(const char* data);
int terminal_write(const char* data, int size);
int terminal_read(char* data, int size);
void terminal_flush_serial(struct terminal* term);
void terminal_putchar(char c);


void terminal_commit();
void twriteln(const char* data);

#endif
