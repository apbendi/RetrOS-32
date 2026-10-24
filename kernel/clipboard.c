/**
 * @file clipboard.c
 * @brief The clipboard: one text that programs and kernel threads copy to and paste from. Programs reach it with
 * system calls, and kernel threads, like the terminal, call it directly.
 */

#include <clipboard.h>
#include <memory.h>
#include <sync.h>
#include <errors.h>
#include <libc.h>
#include <syscalls.h>
#include <syscall_helper.h>

/* The text, which isn't 0-terminated, and its size. Replaced whole, under the lock */
static spinlock_t clipboard_lock = SPINLOCK_UNLOCKED;
static char* clipboard_text = NULL;
static int clipboard_size = 0;

/**
 * Replaces the clipboard's text with a copy of size bytes from data, which can be empty.
 * @return size, or a negative error code, leaving the clipboard as it was: -ERROR_INVALID_ARGUMENTS for more than
 * CLIPBOARD_MAX bytes, or -ERROR_ALLOC without the memory for the copy
 */
int clipboard_set(const char* data, int size)
{
	if(size < 0 || size > CLIPBOARD_MAX || (data == NULL && size > 0)) return -ERROR_INVALID_ARGUMENTS;

	/* Copied before taking the lock, so the lock is never held while allocating */
	char* text = NULL;
	if(size > 0){
		text = kalloc_try(size);
		if(text == NULL) return -ERROR_ALLOC;
		memcpy(text, data, size);
	}

	spin_lock(&clipboard_lock);
	char* old = clipboard_text;
	clipboard_text = text;
	clipboard_size = size;
	spin_unlock(&clipboard_lock);

	kfree(old);
	return size;
}
EXPORT_SYSCALL(SYSCALL_CLIPBOARD_SET, clipboard_set);

/**
 * Copies as much of the clipboard's text into data as fits in size bytes, without a terminating 0. A size of 0 only
 * asks how big the text is.
 * @return the size of the whole text, which is more than size when it didn't all fit, or a negative error code
 */
int clipboard_get(char* data, int size)
{
	if(size < 0 || (data == NULL && size > 0)) return -ERROR_INVALID_ARGUMENTS;

	spin_lock(&clipboard_lock);
	int length = clipboard_size;
	if(length > 0 && size > 0){
		memcpy(data, clipboard_text, length < size ? length : size);
	}
	spin_unlock(&clipboard_lock);

	return length;
}
EXPORT_SYSCALL(SYSCALL_CLIPBOARD_GET, clipboard_get);
