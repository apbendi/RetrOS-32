#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/**
 * A key with a character sends it, and Ctrl+key sends 128 plus the key's character without Shift (drivers/keyboard.c).
 * So a key without a character needs a code that's no Ctrl+key's. 251-254 would be Ctrl with '{', '|', '}' or '~',
 * which only come with Shift.
 */
#define ARROW_UP 254
#define ARROW_DOWN 253
#define ARROW_LEFT 252
#define ARROW_RIGHT 251
/* A character without Shift is never an uppercase letter, so 193-218, 128 plus 'A' to 'Z', can't be a Ctrl+key */
#define F1 193
#define F2 194
#define F3 195
#define F4 196
#define F5 197
#define F6 198
#define F7 199
#define F8 200
#define F9 201
#define F10 202
#define KEY_HOME 203
#define KEY_END 204
#define KEY_PAGE_UP 205
#define KEY_PAGE_DOWN 206
#define KEY_DELETE 207
/* Ctrl+key is 128 plus the key's character, and no key's character is 1-7, so 129-135 can't be a Ctrl+key */
#define F12 134

/* The window server's shortcuts (#89). No key's character is 11-26 either, so 139-154 can't be a Ctrl+key */
#define ALT_TAB 139
#define ALT_F4 140
#define ALT_1 141 /* ALT_1 to ALT_4 are consecutive */
#define ALT_2 142
#define ALT_3 143
#define ALT_4 144

#define TAB 9

#define CTRLA 225
#define CTRLC 227
#define CTRLD 228
#define CTRLE 229
#define CTRLV 246
#define CTRLX 248
#define CTRLY 249
#define CTRLZ 250

/* The modifier keys held with a key, in struct kb_key and gfx_event's flags */
#define KEY_MOD_SHIFT (1 << 0)
#define KEY_MOD_CTRL (1 << 1)
#define KEY_MOD_ALT (1 << 2)

/* A key typed, with the modifier keys held when it was pressed */
struct kb_key {
    unsigned char key;
    unsigned char modifiers;
};

void init_keyboard();
unsigned char kb_get_char(int block);
int kb_get_key(struct kb_key* key);
unsigned char kb_get_modifiers(void);

#endif