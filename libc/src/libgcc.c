/**
 * GCC helpers for 64-bit division that TCC's runtime library, libtcc1.a, doesn't have. GCC calls them for code that
 * needs a quotient and a remainder, like lldiv(). GCC-built programs also have libgcc.a, but programs that TCC links
 * on RetrOS get them only here.
 *
 * They call __divdi3 and __udivdi3, which both libgcc.a and libtcc1.a have, by name: written as a division and a
 * remainder, GCC recognizes the pattern and compiles each into a call to itself.
 */
long long __divdi3(long long a, long long b);
unsigned long long __udivdi3(unsigned long long a, unsigned long long b);
long long __divmoddi4(long long a, long long b, long long* rem);
unsigned long long __udivmoddi4(unsigned long long a, unsigned long long b, unsigned long long* rem);

long long __divmoddi4(long long a, long long b, long long* rem)
{
	long long q = __divdi3(a, b);
	if(rem) *rem = a - q * b;
	return q;
}

unsigned long long __udivmoddi4(unsigned long long a, unsigned long long b, unsigned long long* rem)
{
	unsigned long long q = __udivdi3(a, b);
	if(rem) *rem = a - q * b;
	return q;
}
