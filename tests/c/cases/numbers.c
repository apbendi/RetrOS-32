/* Number parsing: the strtol and strtod families, atof, sscanf, and printing parsed values back out exactly */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

static void long_case(const char* s, int base)
{
	char* end;
	errno = 0;
	long v = strtol(s, &end, base);
	printf("strtol(\"%s\", %d) = %ld, rest \"%s\", ERANGE %d\n", s, base, v, end, errno == ERANGE);
}

static void double_case(const char* s)
{
	char* end;
	errno = 0;
	double d = strtod(s, &end);
	printf("strtod(\"%s\") = %.17g, rest \"%s\", ERANGE %d\n", s, d, end, errno == ERANGE);
}

int main(void)
{
	long_case("12345", 10);
	long_case("  -0x1F rest", 0);
	long_case("0777", 0);
	long_case("z", 36);
	long_case("9999999999999999999", 10);
	long_case("-9999999999999999999", 10);
	long_case("", 10);
	long_case("12abc", 16);

	char* end;
	printf("strtoul %lu\n", strtoul("4294967295", &end, 10));
	printf("strtoll %lld\n", strtoll("-9223372036854775808", &end, 10));
	printf("strtoull %llu\n", strtoull("18446744073709551615", &end, 10));
	printf("strtoull hex %llx\n", strtoull("0xFEDCBA9876543210", &end, 16));

	double_case("3.14159");
	double_case("-2.5e-3");
	double_case("1e308");
	double_case("1e309");
	/* Whether a denormal result sets ERANGE is up to the library: glibc does, musl doesn't */
	printf("strtod(\"4.9e-324\") = %.17g\n", strtod("4.9e-324", 0));
	double_case("1e-400");
	double_case("0x1.8p3");
	double_case("  .5abc");
	double_case("inf");
	double_case("-Infinity");
	double_case("0.1");
	double_case("123456789012345678901234567890");
	double_case("2.2250738585072011e-308");
	printf("strtof %.9g\n", strtof("0.1", 0));
	printf("strtold %.21Lg\n", strtold("0.1", 0));
	printf("atof %g\n", atof("-12.75e2"));

	int a, n;
	unsigned u;
	double d;
	char word[16], ch;
	int got = sscanf("42 0x1f 3.5e2 hello Z", "%d %x %lf %15s %c", &a, &u, &d, word, &ch);
	printf("sscanf %d: %d %u %g %s %c\n", got, a, u, d, word, ch);
	got = sscanf("12:34:56", "%d:%d:%n", &a, (int*)&u, &n);
	printf("sscanf %d: %d %u consumed %d\n", got, a, u, n);
	got = sscanf("abc", "%d", &a);
	printf("sscanf no match %d\n", got);
	got = sscanf("", "%d", &a);
	printf("sscanf empty %d\n", got);
	return 0;
}
