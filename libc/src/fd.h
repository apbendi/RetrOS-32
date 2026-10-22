/**
 * The library's view of file descriptors.
 *
 * RetrOS's file descriptors start at 3, per process. 0 to 2 are stdin, stdout and stderr, which the library handles
 * itself: stdout and stderr write to the terminal, and stdin is at its end, since programs can't read the terminal yet
 * (#113).
 *
 * RetrOS has no fstat for a descriptor, so open() keeps what fstat needs: whether it's a directory, and when the file
 * was last written.
 */
#ifndef RETROS_LIBC_FD_H
#define RETROS_LIBC_FD_H

#include <time.h>
#include "retros.h"

#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_STDERR 2

struct __retros_fd {
	int open;
	/* The flags it was opened with, for fcntl(F_GETFL) */
	int flags;
	int is_directory;
	time_t written;
};

/* What open() kept for fd, or NULL if it's not a file descriptor RetrOS gave */
struct __retros_fd* __retros_fd(int fd);

#endif
