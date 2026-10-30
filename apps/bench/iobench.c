/*
 * A file system benchmark: writes a file of KB kilobytes (8192 unless given) in 4 KB writes, reads it back in 4 KB
 * reads, checking each block, deletes it, and prints how long the writing and reading took. The file is iobench.tmp,
 * in the current directory. The work is the same every run, so the times can be compared.
 *
 *   exec /bin/iobench.o [KB]
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILE_NAME "iobench.tmp"
#define BLOCK 4096

/* Every block is the same pattern, with its number in its first bytes, so reading the wrong block is caught */
static unsigned char expected[BLOCK];
static unsigned char got[BLOCK];

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int fail(const char* what)
{
	perror(what);
	unlink(FILE_NAME);
	return 1;
}

int main(int argc, char* argv[])
{
	int kb = argc > 1 ? atoi(argv[1]) : 8192;
	int blocks = kb / (BLOCK / 1024);
	if (blocks < 1) {
		printf("usage: iobench [KB], at least %d\n", BLOCK / 1024);
		return 1;
	}
	for (int i = 0; i < BLOCK; i++) expected[i] = (unsigned char)(i * 7);

	long start = now_ms();
	int fd = open(FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return fail("open for writing");
	for (int n = 0; n < blocks; n++) {
		memcpy(expected, &n, sizeof n);
		if (write(fd, expected, BLOCK) != BLOCK) return fail("write");
	}
	close(fd);
	long written = now_ms();

	fd = open(FILE_NAME, O_RDONLY);
	if (fd < 0) return fail("open for reading");
	for (int n = 0; n < blocks; n++) {
		memcpy(expected, &n, sizeof n);
		if (read(fd, got, BLOCK) != BLOCK) return fail("read");
		if (memcmp(got, expected, BLOCK) != 0) {
			printf("iobench: block %d read back wrong\n", n);
			close(fd);
			unlink(FILE_NAME);
			return 1;
		}
	}
	close(fd);
	long done = now_ms();
	unlink(FILE_NAME);

	printf("iobench: %d KB, write %ld ms, read %ld ms, total %ld ms\n", blocks * (BLOCK / 1024), written - start,
		done - written, done - start);
	return 0;
}
