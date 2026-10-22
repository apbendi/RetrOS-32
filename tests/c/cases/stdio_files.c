/* Files through stdio: every fopen mode, reading and writing, seeking, lines, ungetc, errors, remove and rename */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void show_file(const char* name)
{
	FILE* f = fopen(name, "r");
	if (!f) {
		printf("%s: can't open, errno %s\n", name, errno == ENOENT ? "ENOENT" : "other");
		return;
	}
	char line[128];
	printf("%s:", name);
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;
		printf(" [%s]", line);
	}
	printf(" eof %d error %d\n", feof(f) != 0, ferror(f) != 0);
	fclose(f);
}

int main(void)
{
	FILE* f = fopen("stdio-a.txt", "w");
	printf("fopen w %d\n", f != 0);
	fprintf(f, "first %d\n", 1);
	fputs("second\n", f);
	fputc('3', f);
	fputc('\n', f);
	printf("ftell %ld\n", ftell(f));
	fclose(f);
	show_file("stdio-a.txt");

	f = fopen("stdio-a.txt", "a");
	fputs("appended\n", f);
	fclose(f);
	show_file("stdio-a.txt");

	f = fopen("stdio-a.txt", "r+");
	fseek(f, 6, SEEK_SET);
	fputs("ONE", f);
	fseek(f, 0, SEEK_END);
	printf("size %ld\n", ftell(f));
	rewind(f);
	char word[16];
	int read = (int)fread(word, 1, 9, f);
	printf("read %d chars [%.9s]\n", read, word);
	int c = fgetc(f);
	printf("fgetc %d\n", c);
	ungetc('Z', f);
	printf("after ungetc %c\n", fgetc(f));
	fclose(f);
	show_file("stdio-a.txt");

	f = fopen("stdio-b.bin", "w+");
	unsigned int numbers[100];
	for (int i = 0; i < 100; i++) numbers[i] = i * 2654435761u;
	printf("fwrite %d\n", (int)fwrite(numbers, sizeof(numbers[0]), 100, f));
	fflush(f);
	fseek(f, 40 * sizeof(numbers[0]), SEEK_SET);
	unsigned int back[10];
	printf("fread %d\n", (int)fread(back, sizeof(back[0]), 10, f));
	printf("matches %d\n", memcmp(back, numbers + 40, sizeof(back)) == 0);
	/* Separate statements: the order a call's arguments are evaluated in is unspecified, and GCC and TCC differ */
	int got = (int)fread(back, sizeof(back[0]), 100, f);
	printf("fread past the end %d, eof %d\n", got, feof(f) != 0);
	clearerr(f);
	printf("after clearerr eof %d\n", feof(f) != 0);
	fclose(f);

	f = fopen("stdio-c.txt", "w");
	for (int i = 0; i < 300; i++) fprintf(f, "line %d of a longer file, to fill a few buffers\n", i);
	fclose(f);
	f = fopen("stdio-c.txt", "r");
	char* line = 0;
	size_t cap = 0;
	ssize_t len;
	int lines = 0;
	long total = 0;
	while ((len = getline(&line, &cap, f)) > 0) {
		lines++;
		total += len;
	}
	printf("getline %d lines, %ld bytes\n", lines, total);
	free(line);
	fclose(f);

	FILE* missing = fopen("no-such.txt", "r");
	printf("fopen a missing file %d errno ENOENT %d\n", missing == 0, errno == ENOENT);
	printf("rename onto an existing file %d\n", rename("stdio-a.txt", "stdio-c.txt"));
	show_file("stdio-c.txt");
	show_file("stdio-a.txt");
	int removed_c = remove("stdio-c.txt");
	int removed_b = remove("stdio-b.bin");
	printf("remove %d %d\n", removed_c, removed_b);
	int removed = remove("stdio-c.txt");
	printf("remove again %d errno ENOENT %d\n", removed, errno == ENOENT);

	f = fopen("stdio-d.txt", "w");
	setvbuf(f, 0, _IONBF, 0);
	fputs("unbuffered\n", f);
	fclose(f);
	show_file("stdio-d.txt");
	remove("stdio-d.txt");
	return 0;
}
