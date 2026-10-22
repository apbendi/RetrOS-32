/* printf's conversions: integers of every size, strings, characters, floating point, widths, precision and flags */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <limits.h>
#include <float.h>

int main(void)
{
	printf("[%d] [%i] [%u] [%x] [%X] [%o] [%c] [%s] [%%]\n", -42, 42, 42u, 255, 255, 8, 'A', "text");
	printf("[%5d] [%-5d] [%05d] [%+d] [% d] [%+d]\n", 42, 42, 42, 42, 42, -42);
	printf("[%#x] [%#o] [%#X] [%x]\n", 255, 8, 0xabc, 0);
	printf("[%.3d] [%8.3d] [%-8.3d] [%.0d]\n", 7, 7, 7, 0);
	printf("[%10s] [%-10s] [%.2s] [%*d] [%-*d] [%.*s]\n", "right", "left", "cut", 6, 9, 6, 9, 3, "precision");
	printf("[%d] [%d] [%u]\n", INT_MAX, INT_MIN, UINT_MAX);
	printf("[%ld] [%lu] [%lx]\n", LONG_MAX, ULONG_MAX, 0xdeadbeefUL);
	printf("[%lld] [%lld] [%llu] [%llx] [%llo]\n", LLONG_MAX, LLONG_MIN, ULLONG_MAX, 0x123456789abcdefULL, 01234567012345670ULL);
	printf("[%hd] [%hu] [%hhd] [%hhu]\n", (short)-1234, (unsigned short)65535, (signed char)-12, (unsigned char)250);
	printf("[%zu] [%zd] [%jd] [%td]\n", (size_t)123456, (ssize_t)-5, (intmax_t)-9000000000LL, (ptrdiff_t)-3);

	printf("[%f] [%.2f] [%10.3f] [%-10.1f] [%+.1f] [%.0f] [%.0f] [%.0f]\n", 3.14159265, 2.675, -1.5, 0.25, 7.0, 0.5, 1.5, 2.5);
	printf("[%e] [%.3e] [%E] [%g] [%g] [%g] [%G]\n", 12345.678, 0.000123456, 1e100, 100000.0, 1000000.0, 0.0001, 1e-10);
	printf("[%g] [%.10g] [%.17g] [%#g] [%g]\n", 1.0 / 3.0, 1.0 / 3.0, 0.1, 1.0, -0.0);
	printf("[%f] [%e] [%g]\n", 1e300, 1e-300, 123456789.0);
	printf("[%.20f] [%.15e]\n", 1.0 / 7.0, 2.0 / 3.0);
	printf("[%f] [%f] [%F] [%e]\n", 1.0 / 0.0, -1.0 / 0.0, 1.0 / 0.0, -1.0 / 0.0);
	printf("[%.3f] [%.3f] [%g] [%g]\n", DBL_MAX / 1e300, DBL_MIN * 1e300, FLT_MAX, (double)FLT_MIN);
	printf("[%Lf] [%.10Le] [%Lg]\n", 1.0L / 3.0L, 1e1000L, 12.5L);
	printf("[%a] [%a] [%A]\n", 1.0, 0.5, 255.0);

	char buf[16];
	int n = snprintf(buf, sizeof(buf), "%s-%d-%s", "abcdef", 12345, "ghijkl");
	printf("snprintf %d [%s]\n", n, buf);
	n = snprintf(0, 0, "%d %s", 1000000, "counted");
	printf("snprintf counting %d\n", n);
	n = sprintf(buf, "%05.1f", 3.14159);
	printf("sprintf %d [%s]\n", n, buf);

	int count = 0;
	printf("abc%ndef\n", &count);
	printf("%%n %d\n", count);
	puts("puts adds a newline");
	putchar('x');
	putchar('\n');
	fputs("fputs to stdout\n", stdout);
	fprintf(stdout, "fprintf %s %d\n", "to stdout", 7);
	return 0;
}
