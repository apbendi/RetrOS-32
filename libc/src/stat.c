/* stat(), fstat(), lstat() and mkdir(), on RetrOS's stat and mkdir system calls */
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "fd.h"

/* A number for each path, standing in for an inode number, so two different files don't look the same.
 * Names ignore case on FAT, so the hash does too. Two spellings of one path can still differ. */
static ino_t __path_ino(const char* path)
{
	unsigned int hash = 2166136261u;
	for (; *path; path++){
		unsigned char c = *path;
		if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
		hash = (hash ^ c) * 16777619u;
	}
	return hash ? hash : 1;
}

static void __fill_stat(struct stat* st, int is_directory, int read_only, off_t size, time_t written)
{
	memset(st, 0, sizeof(*st));
	st->st_mode = is_directory ? S_IFDIR | 0755 : S_IFREG | (read_only ? 0444 : 0644);
	st->st_nlink = 1;
	st->st_size = size;
	st->st_blksize = 512;
	st->st_blocks = (size + 511) / 512;
	st->st_atim.tv_sec = st->st_mtim.tv_sec = st->st_ctim.tv_sec = written;
}

int stat(const char* restrict path, struct stat* restrict st)
{
	struct fs_dirent entry;
	int ret = __retros_ret(__retros_syscall(SYSCALL_STAT, (int)path, (int)&entry, 0));
	if(ret < 0) return -1;

	__fill_stat(st, entry.attributes & FS_ATTR_DIRECTORY, entry.attributes & FS_ATTR_READ_ONLY, entry.size,
		__retros_time(&entry.written));
	st->st_ino = __path_ino(path);
	return 0;
}

/* RetrOS has no links */
int lstat(const char* restrict path, struct stat* restrict st)
{
	return stat(path, st);
}

int fstat(int fd, struct stat* st)
{
	if(fd >= FD_STDIN && fd <= FD_STDERR){
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IFCHR | 0620;
		st->st_nlink = 1;
		st->st_ino = fd + 1;
		return 0;
	}

	struct __retros_fd* kept = __retros_fd(fd);
	if(kept == 0 || !kept->open){
		errno = EBADF;
		return -1;
	}

	/* The size is wherever the end is now: seek there and back */
	off_t here = lseek(fd, 0, SEEK_CUR);
	off_t size = here < 0 ? -1 : lseek(fd, 0, SEEK_END);
	if(here < 0 || size < 0 || lseek(fd, here, SEEK_SET) < 0) return -1;

	__fill_stat(st, kept->is_directory, 0, kept->is_directory ? 0 : size, kept->written);
	/* Unique among the program's open files, though not the same number stat() gives */
	st->st_ino = 0x80000000u | fd;
	return 0;
}

int mkdir(const char* path, mode_t mode)
{
	(void)mode;
	return __retros_ret(__retros_syscall(SYSCALL_MKDIR, (int)path, 0, 0));
}
