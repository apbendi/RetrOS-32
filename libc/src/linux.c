/**
 * The Linux system calls musl's own code makes, done with the library's RetrOS functions (arch/syscall_arch.h).
 * They return what Linux would: the result, or a negative errno.
 */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "fd.h"

int __retros_rename(const char* old, const char* new);

/* The terminal's size, for programs that ask: a window shows about 80 columns by 25 lines at 640x480 */
static long __ioctl(int fd, int request, void* arg)
{
	if(request != TIOCGWINSZ) return -EINVAL;
	if(fd < FD_STDIN || fd > FD_STDERR) return __retros_fd(fd) && __retros_fd(fd)->open ? -ENOTTY : -EBADF;

	struct winsize* size = arg;
	size->ws_row = 25;
	size->ws_col = 80;
	size->ws_xpixel = size->ws_ypixel = 0;
	return 0;
}

static long __fcntl(int fd, int command, long arg)
{
	struct __retros_fd* kept = __retros_fd(fd);
	int known = (fd >= FD_STDIN && fd <= FD_STDERR) || (kept && kept->open);
	if(!known) return -EBADF;

	switch(command){
	case F_GETFD:
	case F_SETFD:
		/* RetrOS doesn't exec, so close-on-exec has nothing to do */
		return 0;
	case F_GETFL:
		if(fd == FD_STDIN) return O_RDONLY;
		if(fd <= FD_STDERR) return O_WRONLY;
		return kept->flags;
	case F_SETFL:
		/* RetrOS can't change a file's flags once it's open */
		return (arg & O_APPEND) == (kept && fd > FD_STDERR ? kept->flags & O_APPEND : 0) ? 0 : -EINVAL;
	default:
		return -EINVAL;
	}
}

long __retros_linux_syscall(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	long ret;
	switch(n){
	case SYS_read: ret = read(a1, (void*)a2, a3); break;
	case SYS_write: ret = write(a1, (const void*)a2, a3); break;
	case SYS_readv: ret = readv(a1, (const struct iovec*)a2, a3); break;
	case SYS_writev: ret = writev(a1, (const struct iovec*)a2, a3); break;
	case SYS_open: ret = open((const char*)a1, a2, a3); break;
	case SYS_close: ret = close(a1); break;
	case SYS_unlink: ret = unlink((const char*)a1); break;
	case SYS_rmdir: ret = rmdir((const char*)a1); break;
	case SYS_rename: ret = __retros_rename((const char*)a1, (const char*)a2); break;
	case SYS_ioctl: return __ioctl(a1, a2, (void*)a3);
	case SYS_fcntl:
	case SYS_fcntl64: return __fcntl(a1, a2, a3);
	case SYS_exit:
	case SYS_exit_group: _exit(a1);
	default: return -ENOSYS;
	}
	return ret < 0 ? -errno : ret;
}
