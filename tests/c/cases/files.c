/* Files through the POSIX calls: create, write, read back, seek, stat, append, truncate, errors and directories */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "harness.h"

static const char* error_name(int e)
{
	switch (e) {
	case 0: return "none";
	case ENOENT: return "ENOENT";
	case EEXIST: return "EEXIST";
	case EISDIR: return "EISDIR";
	case ENOTDIR: return "ENOTDIR";
	case ENOTEMPTY: return "ENOTEMPTY";
	case EBADF: return "EBADF";
	case EINVAL: return "EINVAL";
	default: return "other";
	}
}

static void show_error(const char* label, int ret)
{
	out(label); out(" "); out_num(ret); out(" "); out(ret < 0 ? error_name(errno) : "none"); out("\n");
	errno = 0;
}

int main(void)
{
	char buf[256];
	struct stat st;

	int fd = open("files-test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	show("opened", fd >= 0);
	show("write", write(fd, "line one\n", 9));
	show("write", write(fd, "line two\n", 9));
	close(fd);

	fd = open("files-test.txt", O_RDONLY);
	int n = read(fd, buf, sizeof(buf));
	show("read", n);
	buf[n] = 0;
	out(buf);
	show("read at the end", read(fd, buf, sizeof(buf)));
	show("seek to 5", lseek(fd, 5, SEEK_SET));
	n = read(fd, buf, 3);
	buf[n] = 0;
	out(buf); out("\n");
	show("seek from the end", lseek(fd, -4, SEEK_END));
	show("seek from here", lseek(fd, 2, SEEK_CUR));
	show("fstat size", fstat(fd, &st) == 0 ? (long long)st.st_size : -1);
	show("fstat regular", S_ISREG(st.st_mode));
	close(fd);

	fd = open("files-test.txt", O_WRONLY | O_APPEND);
	write(fd, "line three\n", 11);
	close(fd);
	show("stat after append", stat("files-test.txt", &st) == 0 ? (long long)st.st_size : -1);

	fd = open("files-test.txt", O_RDWR);
	lseek(fd, 5, SEEK_SET);
	write(fd, "ONE", 3);
	lseek(fd, 0, SEEK_SET);
	n = read(fd, buf, sizeof(buf));
	buf[n] = 0;
	out(buf);
	close(fd);

	fd = open("files-test.txt", O_WRONLY | O_TRUNC);
	close(fd);
	show("stat after truncate", stat("files-test.txt", &st) == 0 ? (long long)st.st_size : -1);

	show_error("O_EXCL on an existing file", open("files-test.txt", O_WRONLY | O_CREAT | O_EXCL, 0644));
	show_error("open a missing file", open("no-such-file.txt", O_RDONLY));
	show_error("open in a missing directory", open("no-such-dir/x.txt", O_WRONLY | O_CREAT, 0644));
	show_error("stat a missing file", stat("no-such-file.txt", &st));
	show_error("read a bad descriptor", read(99, buf, 1));

	show_error("mkdir", mkdir("files-dir", 0755));
	show_error("mkdir again", mkdir("files-dir", 0755));
	show("stat a directory", stat("files-dir", &st) == 0 && S_ISDIR(st.st_mode));
	fd = open("files-dir/inner.txt", O_WRONLY | O_CREAT, 0644);
	write(fd, "x\n", 2);
	close(fd);
	show_error("rmdir a full directory", rmdir("files-dir"));
	show_error("unlink a directory", unlink("files-dir"));
	show_error("unlink", unlink("files-dir/inner.txt"));
	show_error("rmdir", rmdir("files-dir"));
	show_error("unlink", unlink("files-test.txt"));
	show_error("unlink again", unlink("files-test.txt"));
	return 0;
}
