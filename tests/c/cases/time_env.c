/* Calendar conversions of fixed times, strftime, the environment, sysconf and realpath. Nothing here depends on
 * the actual time or directory, so it's the same on Linux and RetrOS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

static void show_time(time_t t)
{
	struct tm tm;
	gmtime_r(&t, &tm);
	char buf[64];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %a %b %j", &tm);
	printf("%lld: %s, wday %d yday %d, timegm back %lld\n", (long long)t, buf, tm.tm_wday, tm.tm_yday,
		(long long)timegm(&tm));
}

int main(void)
{
	show_time(0);
	show_time(951782400);
	show_time(1000000000);
	show_time(2147483647);
	show_time(4102444800LL);

	struct tm tm = { 0 };
	tm.tm_year = 124;
	tm.tm_mon = 1;
	tm.tm_mday = 29;
	tm.tm_hour = 23;
	tm.tm_min = 59;
	tm.tm_sec = 60;
	time_t t = timegm(&tm);
	printf("2024-02-29 23:59:60 normalizes to %lld, %04d-%02d-%02d %02d:%02d\n", (long long)t, tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
	char buf[64];
	strftime(buf, sizeof(buf), "%A, %B %d, %Y", &tm);
	printf("strftime %s\n", buf);
	printf("asctime %s", asctime(&tm));
	printf("difftime %g\n", difftime(100, 40));

	printf("getenv unset %d\n", getenv("RETROS_TEST_VARIABLE") == 0);
	setenv("RETROS_TEST_VARIABLE", "one", 1);
	printf("getenv %s\n", getenv("RETROS_TEST_VARIABLE"));
	setenv("RETROS_TEST_VARIABLE", "two", 0);
	printf("setenv without overwrite %s\n", getenv("RETROS_TEST_VARIABLE"));
	unsetenv("RETROS_TEST_VARIABLE");
	printf("unsetenv %d\n", getenv("RETROS_TEST_VARIABLE") == 0);

	printf("page size %ld\n", sysconf(_SC_PAGESIZE));

	char cwd[PATH_MAX], resolved[PATH_MAX], expected[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	mkdir("rp-dir", 0755);
	snprintf(expected, sizeof(expected), "%s/rp-dir", strcmp(cwd, "/") ? cwd : "");
	printf("realpath ./rp-dir/../rp-dir/. matches %d\n",
		realpath("./rp-dir/../rp-dir/.", resolved) && strcmp(resolved, expected) == 0);
	printf("realpath of a missing path %d\n", realpath("no-such/thing", resolved) == 0);
	rmdir("rp-dir");
	return 0;
}
