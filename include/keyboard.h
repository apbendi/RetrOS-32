#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define ARROW_UP 254
#define ARROW_DOWN 253
#define ARROW_LEFT 252
#define ARROW_RIGHT 251
#define F1 250
#define F2 249 /* CTRL y */
#define F3 248
#define F4 247 /* CTRL w */
#define F5 246
#define F6 245
#define F7 244 /* CTRL t */
#define F8 243
#define F9 242 /* CTRL r */
#define F10 241 /* CTRL q */
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

#define CTRLC 227
#define CTRLE 229

void init_keyboard();
unsigned char kb_get_char(int block);

#endif