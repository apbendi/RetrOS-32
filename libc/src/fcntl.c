/* open() and creat(), on RetrOS's open system call */
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include "fd.h"

static struct __retros_fd fds[FS_MAX_PROCESS_FILES];

struct __retros_fd* __retros_fd(int fd)
{
	if(fd <= FD_STDERR || fd >= FS_MAX_PROCESS_FILES) return 0;
	return &fds[fd];
}

int open(const char* path, int flags, ...)
{
	int access = flags & O_ACCMODE;
	int retros_flags = 0;
	struct fs_dirent entry;

	if(access == O_RDONLY || access == O_RDWR) retros_flags |= FS_FILE_FLAG_READ;
	if(access == O_WRONLY || access == O_RDWR) retros_flags |= FS_FILE_FLAG_WRITE;
	if(flags & O_CREAT) retros_flags |= FS_FILE_FLAG_CREATE;
	if(flags & O_TRUNC) retros_flags |= FS_FILE_FLAG_TRUNCATE;
	if(flags & O_APPEND) retros_flags |= FS_FILE_FLAG_APPEND;

	/* RetrOS checks neither, so they're checked here first. Another process could still create the file in between. */
	int exists = __retros_syscall(SYSCALL_STAT, (int)path, (int)&entry, 0) == 0;
	if((flags & O_CREAT) && (flags & O_EXCL) && exists){
		errno = EEXIST;
		return -1;
	}
	if((flags & O_DIRECTORY) && !(exists && (entry.attributes & FS_ATTR_DIRECTORY))){
		errno = exists ? ENOTDIR : ENOENT;
		return -1;
	}

	int fd = __retros_ret(__retros_syscall(SYSCALL_OPEN, (int)path, retros_flags, 0));
	if(fd < 0) return -1;

	struct __retros_fd* kept = __retros_fd(fd);
	if(kept != 0){
		/* A new file didn't exist a moment ago, so its time is now */
		kept->open = 1;
		kept->flags = flags & (O_ACCMODE | O_APPEND);
		kept->is_directory = exists && (entry.attributes & FS_ATTR_DIRECTORY);
		kept->written = exists ? __retros_time(&entry.written) : time(0);
	}
	return fd;
}

int creat(const char* path, mode_t mode)
{
	(void)mode;
	return open(path, O_WRONLY | O_CREAT | O_TRUNC);
}
