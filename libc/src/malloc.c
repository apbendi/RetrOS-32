/**
 * malloc, free, calloc and realloc for RetrOS programs.
 *
 * Memory comes from the kernel's malloc system call. The kernel maps pages for each of its allocations and keeps
 * bookkeeping for each in its own heap, so it's asked rarely, for arenas of at least ARENA_SIZE, and the arenas are
 * divided here.
 *
 * Every block starts with a 16-byte header holding its size and the size of the block before it, so a freed block
 * merges with free neighbours on both sides. Free blocks are kept in one list, searched first fit. A request too big
 * for an arena gets a kernel allocation of its own, returned to the kernel when it's freed.
 *
 * Not thread-safe: a program's threads share it without a lock.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "retros.h"

#define ALIGN 16
#define ARENA_SIZE (128 * 1024)
/* Requests over this get their own kernel allocation */
#define BIG_SIZE (64 * 1024)

#define USED 1u
#define BIG 2u
#define FLAGS (USED | BIG)

struct block {
	/* The previous block's size, so free() can find it. For a BIG block, the kernel's pointer instead. */
	size_t prev_size;
	/* This block's size, header included, with USED and BIG in the low bits */
	size_t size;
	/* In a free block: its place in the free list. In a used one, the start of the caller's memory. */
	struct block* next;
	struct block* prev;
};

#define HEADER 16
#define MIN_BLOCK 32

static struct block* free_list;

static size_t __size(const struct block* b)
{
	return b->size & ~(size_t)FLAGS;
}

static struct block* __next(struct block* b)
{
	return (struct block*)((char*)b + __size(b));
}

static void* __memory(struct block* b)
{
	return (char*)b + HEADER;
}

static struct block* __block(void* p)
{
	return (struct block*)((char*)p - HEADER);
}

static void __unlink(struct block* b)
{
	if(b->prev) b->prev->next = b->next;
	else free_list = b->next;
	if(b->next) b->next->prev = b->prev;
}

static void __push(struct block* b)
{
	b->prev = 0;
	b->next = free_list;
	if(free_list) free_list->prev = b;
	free_list = b;
}

static void* __kernel_alloc(size_t size)
{
	return (void*)__retros_syscall(SYSCALL_MALLOC, (int)size, 0, 0);
}

static void __kernel_free(void* p)
{
	__retros_syscall(SYSCALL_FREE, (int)p, 0, 0);
}

/* The block size for a request of n bytes, or 0 if it's too big to represent */
static size_t __need(size_t n)
{
	if(n > SIZE_MAX / 2) return 0;
	if(n == 0) n = 1;
	size_t need = (n + HEADER + ALIGN - 1) & ~(size_t)(ALIGN - 1);
	return need < MIN_BLOCK ? MIN_BLOCK : need;
}

/**
 * Adds an arena with room for a block of at least need bytes. It's laid out as a used header at the start, one free
 * block, and a used header of size 0 at the end, so merging never runs off either end.
 */
static int __grow(size_t need)
{
	size_t size = need + 3 * HEADER + ALIGN;
	if(size < ARENA_SIZE) size = ARENA_SIZE;

	char* raw = __kernel_alloc(size);
	if(raw == 0) return -1;

	/* The kernel's allocations are only 4-byte aligned */
	struct block* start = (struct block*)(((uintptr_t)raw + ALIGN - 1) & ~(uintptr_t)(ALIGN - 1));
	size_t usable = (size - ((char*)start - raw)) & ~(size_t)(ALIGN - 1);

	start->prev_size = 0;
	start->size = HEADER | USED;

	struct block* b = __next(start);
	b->prev_size = HEADER;
	b->size = usable - 2 * HEADER;

	struct block* end = __next(b);
	end->prev_size = b->size;
	end->size = 0 | USED;

	__push(b);
	return 0;
}

/* Cuts block b, which isn't in the free list, down to need bytes, and frees the rest if it's big enough to use */
static void __split(struct block* b, size_t need)
{
	size_t size = __size(b);
	if(size - need < MIN_BLOCK) return;

	b->size = need | (b->size & FLAGS);
	struct block* rest = __next(b);
	rest->prev_size = need;
	rest->size = size - need;

	/* When realloc shrinks a block, the block after it may be free */
	struct block* after = __next(rest);
	if(!(after->size & USED)){
		__unlink(after);
		rest->size += __size(after);
	}
	__next(rest)->prev_size = rest->size;
	__push(rest);
}

void* malloc(size_t n)
{
	size_t need = __need(n);
	if(need == 0){
		errno = ENOMEM;
		return 0;
	}

	if(need > BIG_SIZE){
		char* raw = __kernel_alloc(need + ALIGN);
		if(raw == 0){
			errno = ENOMEM;
			return 0;
		}
		struct block* b = (struct block*)(((uintptr_t)raw + ALIGN - 1) & ~(uintptr_t)(ALIGN - 1));
		b->prev_size = (size_t)raw;
		b->size = need | USED | BIG;
		return __memory(b);
	}

	for(int tries = 0; tries < 2; tries++){
		for(struct block* b = free_list; b; b = b->next){
			if(__size(b) < need) continue;
			__unlink(b);
			b->size |= USED;
			__split(b, need);
			return __memory(b);
		}
		if(__grow(need) < 0) break;
	}
	errno = ENOMEM;
	return 0;
}

void free(void* p)
{
	if(p == 0) return;

	struct block* b = __block(p);
	if(b->size & BIG){
		__kernel_free((void*)b->prev_size);
		return;
	}

	b->size &= ~(size_t)USED;

	struct block* next = __next(b);
	if(!(next->size & USED)){
		__unlink(next);
		b->size += __size(next);
	}

	struct block* prev = (struct block*)((char*)b - b->prev_size);
	if(!(prev->size & USED)){
		__unlink(prev);
		prev->size += __size(b);
		b = prev;
	}

	__next(b)->prev_size = __size(b);
	__push(b);
}

void* calloc(size_t count, size_t size)
{
	if(size && count > SIZE_MAX / size){
		errno = ENOMEM;
		return 0;
	}
	/* The kernel doesn't clear the pages it hands out */
	void* p = malloc(count * size);
	if(p) memset(p, 0, count * size);
	return p;
}

void* realloc(void* p, size_t n)
{
	if(p == 0) return malloc(n);

	size_t need = __need(n);
	if(need == 0){
		errno = ENOMEM;
		return 0;
	}

	struct block* b = __block(p);
	size_t size = __size(b);

	if(!(b->size & BIG)){
		if(need <= size){
			__split(b, need);
			return p;
		}
		/* Grow into the next block if it's free and big enough */
		struct block* next = __next(b);
		if(!(next->size & USED) && size + __size(next) >= need){
			__unlink(next);
			b->size += __size(next);
			__next(b)->prev_size = __size(b);
			__split(b, need);
			return p;
		}
	} else if(need <= size){
		return p;
	}

	void* moved = malloc(n);
	if(moved == 0) return 0;
	memcpy(moved, p, size - HEADER < n ? size - HEADER : n);
	free(p);
	return moved;
}

/* musl's own code calls these names */
void* __libc_malloc(size_t n) { return malloc(n); }
void* __libc_calloc(size_t count, size_t size) { return calloc(count, size); }
void* __libc_realloc(void* p, size_t n) { return realloc(p, n); }
void __libc_free(void* p) { free(p); }
