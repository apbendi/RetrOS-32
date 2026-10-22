/* qsort and bsearch on numbers, strings and structures, and setjmp/longjmp out of nested calls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

static int compare_ints(const void* a, const void* b)
{
	int x = *(const int*)a, y = *(const int*)b;
	return (x > y) - (x < y);
}

static int compare_strings(const void* a, const void* b)
{
	return strcmp(*(const char* const*)a, *(const char* const*)b);
}

struct point { int x, y; };

static int compare_points(const void* a, const void* b)
{
	const struct point* p = a;
	const struct point* q = b;
	if (p->x != q->x) return p->x - q->x;
	return q->y - p->y;
}

static jmp_buf jump;
static int depth;

static void dive(int n)
{
	depth++;
	if (n == 0) longjmp(jump, 42);
	dive(n - 1);
}

int main(void)
{
	int numbers[1000];
	unsigned seed = 1;
	for (int i = 0; i < 1000; i++) {
		seed = seed * 1103515245 + 12345;
		numbers[i] = (int)(seed >> 8) % 10000 - 5000;
	}
	qsort(numbers, 1000, sizeof(int), compare_ints);
	int sorted = 1;
	long long sum = 0;
	for (int i = 0; i < 1000; i++) {
		if (i && numbers[i - 1] > numbers[i]) sorted = 0;
		sum += numbers[i] * (long long)(i + 1);
	}
	printf("ints sorted %d, first %d, last %d, weighted sum %lld\n", sorted, numbers[0], numbers[999], sum);
	int key = numbers[500];
	int* found = bsearch(&key, numbers, 1000, sizeof(int), compare_ints);
	printf("bsearch found %d, value %d\n", found != 0, found ? *found : 0);
	key = 99999;
	printf("bsearch missing %d\n", bsearch(&key, numbers, 1000, sizeof(int), compare_ints) == 0);

	const char* words[] = { "pear", "apple", "fig", "banana", "cherry", "apple", "date" };
	qsort(words, 7, sizeof(words[0]), compare_strings);
	for (int i = 0; i < 7; i++) printf("%s%s", words[i], i < 6 ? " " : "\n");

	struct point points[] = { {3, 1}, {1, 5}, {3, 9}, {2, 2}, {1, 7} };
	qsort(points, 5, sizeof(points[0]), compare_points);
	for (int i = 0; i < 5; i++) printf("(%d,%d)%s", points[i].x, points[i].y, i < 4 ? " " : "\n");

	qsort(numbers, 0, sizeof(int), compare_ints);
	printf("qsort of nothing is fine\n");

	volatile int local = 1;
	int r = setjmp(jump);
	if (r == 0) {
		local = 2;
		dive(50);
		printf("not reached\n");
	}
	printf("longjmp returned %d after %d calls, local %d\n", r, depth, local);

	r = setjmp(jump);
	if (r == 0) longjmp(jump, 0);
	printf("longjmp with 0 gives %d\n", r);
	return 0;
}
