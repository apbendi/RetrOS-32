/* POSIX file and process calls, on RetrOS's system calls (fd.h for descriptors 0 to 2) */
#include <features.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "fd.h"

/* Whether fd isn't a file this program has open. RetrOS calls that invalid arguments, but POSIX says EBADF. */
static int __bad_fd(int fd)
{
	struct __retros_fd* kept = __retros_fd(fd);
	if(kept != 0 && kept->open) return 0;
	errno = EBADF;
	return 1;
}

ssize_t read(int fd, void* buf, size_t count)
{
	if(fd == FD_STDIN) return 0;
	if(fd == FD_STDOUT || fd == FD_STDERR){
		errno = EBADF;
		return -1;
	}
	if(__bad_fd(fd)) return -1;
	return __retros_ret(__retros_syscall(SYSCALL_READ, fd, (int)buf, (int)count));
}

ssize_t write(int fd, const void* buf, size_t count)
{
	if(fd == FD_STDOUT || fd == FD_STDERR){
		return __retros_ret(__retros_syscall(SYSCALL_TERM_WRITE, (int)buf, (int)count, 0));
	}
	if(fd == FD_STDIN){
		errno = EBADF;
		return -1;
	}
	if(__bad_fd(fd)) return -1;
	return __retros_ret(__retros_syscall(SYSCALL_WRITE, fd, (int)buf, (int)count));
}

int close(int fd)
{
	if(fd >= FD_STDIN && fd <= FD_STDERR) return 0;
	if(__bad_fd(fd)) return -1;

	int ret = __retros_ret(__retros_syscall(SYSCALL_CLOSE, fd, 0, 0));
	struct __retros_fd* kept = __retros_fd(fd);
	if(ret == 0 && kept != 0) kept->open = 0;
	return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
	if(fd >= FD_STDIN && fd <= FD_STDERR){
		errno = ESPIPE;
		return -1;
	}
	if(__bad_fd(fd)) return -1;
	/* RetrOS's offsets are ints, and SEEK_SET, SEEK_CUR and SEEK_END are its FS_SEEK_START, _CUR and _END */
	if(offset > INT_MAX || offset < INT_MIN){
		errno = EOVERFLOW;
		return -1;
	}
	return __retros_ret(__retros_syscall(SYSCALL_SEEK, fd, (int)offset, whence));
}

/* musl's stdio calls it by this name */
weak_alias(lseek, __lseek);

int isatty(int fd)
{
	if(fd >= FD_STDIN && fd <= FD_STDERR) return 1;
	errno = __bad_fd(fd) ? EBADF : ENOTTY;
	return 0;
}

int unlink(const char* path)
{
	return __retros_ret(__retros_syscall(SYSCALL_UNLINK, (int)path, 0, 0));
}

int rmdir(const char* path)
{
	return __retros_ret(__retros_syscall(SYSCALL_RMDIR, (int)path, 0, 0));
}

int chdir(const char* path)
{
	return __retros_ret(__retros_syscall(SYSCALL_CHDIR, (int)path, 0, 0));
}

char* getcwd(char* buf, size_t size)
{
	/* Without a buffer, as glibc and musl allow, it's allocated */
	if(buf == 0){
		char tmp[FS_PATH_MAX];
		return getcwd(tmp, sizeof(tmp)) ? strdup(tmp) : 0;
	}
	if(size == 0){
		errno = EINVAL;
		return 0;
	}

	int ret = __retros_syscall(SYSCALL_GETCWD, (int)buf, (int)size, 0);
	if(ret < 0){
		/* RetrOS says the buffer is too small as invalid arguments */
		errno = ret == -ERROR_INVALID_ARGUMENTS ? ERANGE : __retros_errno(ret);
		return 0;
	}
	return buf;
}

int access(const char* path, int mode)
{
	struct stat st;
	if(stat(path, &st) < 0) return -1;
	/* RetrOS has no permissions, but FAT's read-only attribute */
	if((mode & W_OK) && !(st.st_mode & S_IWUSR)){
		errno = EACCES;
		return -1;
	}
	return 0;
}

_Noreturn void _exit(int status)
{
	(void)status;
	/* RetrOS doesn't keep exit codes */
	__retros_syscall(SYSCALL_EXIT, 0, 0, 0);
	for(;;);
}

_Noreturn void _Exit(int status)
{
	_exit(status);
}
