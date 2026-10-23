#ifndef TERMINAL_H
#define TERMINAL_H

#include <pcb.h>
#include <kutils.h>
#include <colors.h>
#include <gfx/window.h>
#include <libc.h>

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

    /* The line being typed, drawn after the text: see terminal_set_input. input_shown is false when there's none */
    char* input;
    int input_size;
    int input_length;
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

    struct window* screen;
    struct terminal_ops* ops;
    color_t text_color;
    color_t bg_color;
};

struct terminal* terminal_create(terminal_flags_t flags);
void terminal_get(struct terminal* term);
void terminal_put(struct terminal* term);
void terminal_lock(struct terminal* term);
void terminal_unlock(struct terminal* term);
void terminal_release_window(struct terminal* term, struct pcb* pcb);
void terminal_set_input(struct terminal* term, const char* prompt, const char* text);
void terminal_hide_input(struct terminal* term);
void terminal_end_line(struct terminal* term);
void terminal_invalidate(struct terminal* term);

int scan(ubyte_t* data, int size);

void twrite(const char* data);
int terminal_write(const char* data, int size);
int terminal_read(char* data, int size);
void terminal_flush_serial(struct terminal* term);
void terminal_putchar(char c);


void terminal_commit();
void twriteln(const char* data);

#endif
