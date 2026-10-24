#ifndef __CLIPBOARD_H
#define __CLIPBOARD_H

/* The most the clipboard holds: the Editor's whole text */
#define CLIPBOARD_MAX (64 * 1024)

int clipboard_set(const char* data, int size);
int clipboard_get(char* data, int size);

#endif /* __CLIPBOARD_H */
