/*
 * A CPU-bound benchmark: finds the primes below 1,000,000 with a sieve, ROUNDS times (1000 unless given), and prints
 * how long that took. The work is the same every run, so the times can be compared.
 *
 *   exec /bin/cpubench.o [ROUNDS]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIMIT 1000000
#define SQRT_LIMIT 1000

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(int argc, char* argv[])
{
	int rounds = argc > 1 ? atoi(argv[1]) : 1000;
	char* composite = malloc(LIMIT);
	if (rounds < 1 || !composite) {
		printf("usage: cpubench [ROUNDS]\n");
		return 1;
	}

	long start = now_ms();
	int primes = 0;
	for (int r = 0; r < rounds; r++) {
		memset(composite, 0, LIMIT);
		primes = 0;
		for (int i = 2; i < LIMIT; i++) {
			if (composite[i]) continue;
			primes++;
			if (i < SQRT_LIMIT)
				for (int j = i * i; j < LIMIT; j += i) composite[j] = 1;
		}
	}
	long ms = now_ms() - start;

	printf("cpubench: %d rounds, %d primes, %ld ms\n", rounds, primes, ms);
	free(composite);
	return 0;
}
