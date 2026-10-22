/* readv() and writev(), from read() and write(). musl's stdio writes with writev. */
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

ssize_t readv(int fd, const struct iovec* iov, int count)
{
	ssize_t total = 0;
	for (int i = 0; i < count; i++){
		if(iov[i].iov_len == 0) continue;
		ssize_t n = read(fd, iov[i].iov_base, iov[i].iov_len);
		if(n < 0) return total ? total : -1;
		total += n;
		if((size_t)n < iov[i].iov_len) break;
	}
	return total;
}

ssize_t writev(int fd, const struct iovec* iov, int count)
{
	/* Small writes go out as one, so the terminal is repainted once (#118) and a file gets one write */
	char joined[4096];
	size_t length = 0;
	for (int i = 0; i < count; i++) length += iov[i].iov_len;
	if(length <= sizeof(joined)){
		size_t at = 0;
		for (int i = 0; i < count; i++){
			memcpy(joined + at, iov[i].iov_base, iov[i].iov_len);
			at += iov[i].iov_len;
		}
		return write(fd, joined, length);
	}

	ssize_t total = 0;
	for (int i = 0; i < count; i++){
		if(iov[i].iov_len == 0) continue;
		ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);
		if(n < 0) return total ? total : -1;
		total += n;
		if((size_t)n < iov[i].iov_len) break;
	}
	return total;
}
