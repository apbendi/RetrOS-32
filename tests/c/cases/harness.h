/**
 * Output helpers for the C harness's programs (tests/c), which print what they find so the harness can compare the
 * output on RetrOS with the same program's on Linux. They write with write(), so they don't depend on stdio.
 *
 */
#include <string.h>
#include <unistd.h>

static void out(const char* s)
{
	write(1, s, strlen(s));
}

static void out_num(long long n)
{
	char buf[24];
	int i = sizeof(buf);
	unsigned long long u = n < 0 ? -(unsigned long long)n : (unsigned long long)n;
	buf[--i] = 0;
	do {
		buf[--i] = '0' + u % 10;
		u /= 10;
	} while (u);
	if (n < 0) buf[--i] = '-';
	out(buf + i);
}

/* "label value" on a line of its own */
static void show(const char* label, long long value)
{
	out(label);
	out(" ");
	out_num(value);
	out("\n");
}
