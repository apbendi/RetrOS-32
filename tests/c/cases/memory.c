/* malloc, calloc, realloc and free: contents survive, alignment, and many sizes over many rounds. It stays under
 * about 1 MB at a time: RetrOS gives a process at most 4 MB, from pages all processes share. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "harness.h"

#define COUNT 200

static unsigned char* blocks[COUNT];
static size_t sizes[COUNT];

static int intact(int i)
{
	for (size_t k = 0; k < sizes[i]; k++) {
		if (blocks[i][k] != (unsigned char)(i * 7 + k)) return 0;
	}
	return 1;
}

static void fill(int i)
{
	for (size_t k = 0; k < sizes[i]; k++) blocks[i][k] = (unsigned char)(i * 7 + k);
}

int main(void)
{
	int bad = 0, misaligned = 0, failed = 0;
	unsigned long seed = 12345;

	for (int round = 0; round < 40; round++) {
		for (int i = 0; i < COUNT; i++) {
			seed = seed * 1103515245 + 12345;
			size_t n = (seed >> 8) % (round % 4 == 3 ? 8000 : 500) + 1;
			if (blocks[i] && !intact(i)) bad++;
			if (blocks[i] && (seed & 1)) {
				unsigned char* p = realloc(blocks[i], n);
				if (!p) { failed++; continue; }
				size_t keep = sizes[i] < n ? sizes[i] : n;
				for (size_t k = 0; k < keep; k++) if (p[k] != (unsigned char)(i * 7 + k)) bad++;
				blocks[i] = p;
			} else {
				free(blocks[i]);
				blocks[i] = malloc(n);
				if (!blocks[i]) { failed++; continue; }
			}
			if (((uintptr_t)blocks[i] & 15) != 0) misaligned++;
			sizes[i] = n;
			fill(i);
		}
	}
	for (int i = 0; i < COUNT; i++) {
		if (blocks[i] && !intact(i)) bad++;
		free(blocks[i]);
	}
	show("damaged bytes", bad);
	show("misaligned blocks", misaligned);
	show("failed allocations", failed);

	int* z = calloc(5000, sizeof(int));
	int nonzero = 0;
	for (int i = 0; i < 5000; i++) nonzero += z[i] != 0;
	show("calloc nonzero", nonzero);
	free(z);

	char* big = malloc(1000000);
	show("a 1 MB block", big != 0);
	if (big) {
		memset(big, 0x5a, 1000000);
		big = realloc(big, 1500000);
		show("grown to 1.5 MB, contents kept", big && big[999999] == 0x5a);
		free(big);
	}
	show("free(NULL) is fine", (free(0), 1));
	show("realloc(NULL) allocates", ({ void* p = realloc(0, 10); int ok = p != 0; free(p); ok; }));
	return 0;
}
