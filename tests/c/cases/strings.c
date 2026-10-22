/* String and character functions: lengths, comparisons, copies, searches, tokens and character classes */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "harness.h"

static int sign(int n) { return n < 0 ? -1 : n > 0; }

int main(void)
{
	char buf[64];

	show("strlen", strlen("hello, world"));
	show("strlen empty", strlen(""));
	show("strcmp equal", sign(strcmp("abc", "abc")));
	show("strcmp less", sign(strcmp("abc", "abd")));
	show("strcmp prefix", sign(strcmp("ab", "abc")));
	show("strcmp high bytes", sign(strcmp("\x80", "\x7f")));
	show("strncmp", sign(strncmp("abcdef", "abcxyz", 3)));
	show("strcasecmp", sign(strcasecmp("Hello", "hELLO")));

	strcpy(buf, "abc");
	strcat(buf, "def");
	out(buf); out("\n");
	strncpy(buf, "xy", 5);
	show("strncpy pads with zeros", buf[2] == 0 && buf[3] == 0 && buf[4] == 0);

	const char* s = "the quick brown fox";
	show("strchr", strchr(s, 'q') - s);
	show("strrchr", strrchr(s, 'o') - s);
	show("strchr missing", strchr(s, 'z') == 0);
	show("strstr", strstr(s, "brown") - s);
	show("strstr missing", strstr(s, "cat") == 0);
	show("strspn", strspn("aaabbbccc", "ab"));
	show("strcspn", strcspn("hello world", " "));
	show("strpbrk", strpbrk(s, "xyz") - s);

	char move[16] = "0123456789";
	memmove(move + 2, move, 8);
	out(move); out("\n");
	memmove(move, move + 3, 5);
	out(move); out("\n");
	show("memcmp", sign(memcmp("abc\0x", "abc\0y", 5)));
	memset(buf, 'z', 10);
	buf[10] = 0;
	out(buf); out("\n");

	char tokens[] = "a,b,,c;d";
	for (char* t = strtok(tokens, ",;"); t; t = strtok(0, ",;")) {
		out("token "); out(t); out("\n");
	}

	char* dup = strdup("duplicate");
	out(dup); out("\n");
	free(dup);

	/* Character classes for every ASCII character, as bit masks summed by class */
	long long alpha = 0, digit = 0, space = 0, punct = 0, upper = 0, lower = 0, print = 0, cntrl = 0, xdigit = 0;
	for (int c = 0; c < 128; c++) {
		alpha += isalpha(c) ? c : 0;
		digit += isdigit(c) ? c : 0;
		space += isspace(c) ? c : 0;
		punct += ispunct(c) ? c : 0;
		upper += isupper(c) ? c : 0;
		lower += islower(c) ? c : 0;
		print += isprint(c) ? c : 0;
		cntrl += iscntrl(c) ? c : 0;
		xdigit += isxdigit(c) ? c : 0;
	}
	show("isalpha", alpha); show("isdigit", digit); show("isspace", space); show("ispunct", punct);
	show("isupper", upper); show("islower", lower); show("isprint", print); show("iscntrl", cntrl);
	show("isxdigit", xdigit);
	show("toupper", toupper('q')); show("tolower", tolower('Q')); show("toupper of a digit", toupper('7'));

	show("atoi", atoi("  -1234xyz"));
	show("atol", atol("2147483647"));
	show("atoll", atoll("-9000000000"));
	show("abs", abs(-5)); show("labs", labs(-70000L)); show("llabs", llabs(-5000000000LL));
	lldiv_t d = lldiv(-1234567890123LL, 1000);
	show("lldiv quot", d.quot); show("lldiv rem", d.rem);
	return 0;
}
