/**
 * @file timer.c
 * @author Joe Bayer (joexbayer)
 * @brief PIT driver, used for preemptive scheduling and timing.
 * @version 0.1
 * @date 2022-06-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <timer.h>
#include <serial.h>
#include <arch/interrupts.h>
#include <scheduler.h>
#include <pcb.h>
#include <arch/io.h>
#include <kutils.h>
#include <syscalls.h>
#include <syscall_helper.h>
#include <libc.h>

#define PIT_IRQ		32

/* The PIT's input clock, in Hz */
#define PIT_HZ		1193182
/* The TSC is measured while PIT channel 2 counts down this many of the PIT's cycles: 50 ms */
#define TSC_CAL_PIT_CYCLES	59659
/* How many times to read channel 2's output before giving up on it, far more than 50 ms takes */
#define TSC_CAL_MAX_POLLS	10000000

static unsigned long interrupts = 0;
static void __int_handler timer_callback()
{
	interrupts++;
	$process->current->preempts++;
	EOI(32);
	if($process->current != NULL)
	{
		kernel_yield();
	}
}

/**
 * The clock reads the CPU's time stamp counter (TSC), since interrupts can be lost and the TSC can't (#193). Set once
 * at boot, before interrupts are on: whether the CPU has a TSC, the TSC when the timer started, and milliseconds per
 * TSC cycle, times 2^40. tsc_mult is 0 without a TSC whose rate could be measured, and the clock counts interrupts.
 */
static int tsc_present = 0;
static unsigned long long tsc_start = 0;
static uint32_t tsc_mult = 0;

/* Whether the CPU has a TSC (CPUID leaf 1, EDX bit 4). A CPU that can't flip EFLAGS' ID bit has no CPUID. */
static int __cpu_has_tsc()
{
	uint32_t before, after;
	asm volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %1; pushl %1; popfl; pushfl; popl %1; pushl %0; popfl"
		: "=&r"(before), "=&r"(after) : : "cc");
	if(((before ^ after) & 0x200000) == 0) return 0;

	uint32_t eax = 1, ebx, ecx, edx;
	asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
	(void)ebx;
	(void)ecx;
	return (edx >> 4) & 1;
}

/* n / d, for a quotient whose high half is high / d: the kernel has no 64-bit division helper, but divl does the rest */
static unsigned long long __div64_32(unsigned long long n, uint32_t d)
{
	uint32_t high = (uint32_t)(n >> 32);
	uint32_t rem = high % d;
	uint32_t low = (uint32_t)n;
	asm("divl %2" : "+a"(low), "+d"(rem) : "rm"(d));
	return ((unsigned long long)(high / d) << 32) | low;
}

/* Milliseconds in a number of TSC cycles: (cycles * tsc_mult) >> 40. The product needs 96 bits, so it's made of two
 * 32-bit multiplies: (high * 2^32 + low) >> 40 is (high >> 8), plus the rest of high and low shifted down. */
static unsigned long long __tsc_to_ms(unsigned long long cycles)
{
	unsigned long long low = (unsigned long long)(uint32_t)cycles * tsc_mult;
	unsigned long long high = (unsigned long long)(uint32_t)(cycles >> 32) * tsc_mult;
	return (high >> 8) + ((((high & 0xFF) << 24) + (low >> 8)) >> 32);
}

/**
 * Measures the TSC's rate against the PIT, reading channel 2 rather than counting the PIT's interrupts, which can be
 * lost: channel 2 counts down TSC_CAL_PIT_CYCLES, 50 ms, once, and its output, bit 5 of port 0x61, goes high at the
 * end. Linux measures it the same way. Runs at boot, with interrupts still off.
 */
static void __timer_calibrate_tsc()
{
	tsc_present = __cpu_has_tsc();
	if(!tsc_present){
		dbgprintf("[TIMER] No TSC, so the clock counts timer interrupts.\n");
		return;
	}

	/* Channel 2's gate on and the speaker off, then channel 2 in mode 0, counting down once, from the low and high byte */
	outportb(0x61, (inportb(0x61) & ~0x02) | 0x01);
	outportb(0x43, 0xB0);
	outportb(0x42, TSC_CAL_PIT_CYCLES & 0xFF);
	outportb(0x42, TSC_CAL_PIT_CYCLES >> 8);

	unsigned long long start = rdtsc();
	uint32_t polls = 0;
	while((inportb(0x61) & 0x20) == 0 && polls < TSC_CAL_MAX_POLLS){
		polls++;
	}
	unsigned long long cycles = rdtsc() - start;

	/* An output that was high already, or never went high, means channel 2 didn't count: there's no rate to trust */
	if(polls < 2 || polls == TSC_CAL_MAX_POLLS || (cycles >> 32) != 0 || cycles < 50000){
		dbgprintf("[TIMER] Couldn't measure the TSC (%d polls), so the clock counts timer interrupts.\n", polls);
		return;
	}

	/* 2^40 times the 50 ms measured, over the cycles counted. The constant is folded at compile time. */
	tsc_mult = (uint32_t)__div64_32((((1ULL << 40) * TSC_CAL_PIT_CYCLES) / PIT_HZ) * 1000, (uint32_t)cycles);

	uint32_t khz = (uint32_t)__div64_32(cycles * PIT_HZ, TSC_CAL_PIT_CYCLES * 1000);
	dbgprintf("[TIMER] TSC: %d.%d%d%d MHz, measured against the PIT.\n",
		khz / 1000, khz % 1000 / 100, khz % 100 / 10, khz % 10);
}

_Static_assert(1000 % TIMER_HZ == 0, "Each timer interrupt has to be a whole number of milliseconds");

/**
 * Milliseconds since the timer started: the clock that sleeps, timeouts and CLOCK_MONOTONIC read. It's read from the
 * TSC, so interrupts lost along the way don't slow it down. Without a TSC, it counts the timer's interrupts, each
 * 1000 / TIMER_HZ milliseconds.
 */
uint32_t timer_get_ms()
{
	if(tsc_mult == 0){
		return interrupts * (1000 / TIMER_HZ);
	}
	return (uint32_t)__tsc_to_ms(rdtsc() - tsc_start);
}
EXPORT_SYSCALL(SYSCALL_GET_MS, timer_get_ms);

/* Reads the TSC into tsc, if the CPU has one. Returns whether it does. */
int timer_get_tsc(unsigned long long* tsc)
{
	if(!tsc_present) return 0;
	*tsc = rdtsc();
	return 1;
}

/* The timer interrupts handled since the timer started, which preempt threads, for uptime */
int timer_get_interrupts()
{
	return interrupts;
}

int time_get_difference(struct time* t1, struct time* t2)
{
	uint32_t time1 = (t1->hour*3600) + (t1->minute*60) + t1->second;
	uint32_t time2 = (t2->hour*3600) + (t2->minute*60) + t2->second;

	return time1 - time2;
}

/* http://www.jamesmolloy.co.uk/tutorial_html/5.-IRQs%20and%20the%20PIT.html */
void init_pit(uint32_t frequency)
{
	__timer_calibrate_tsc();

	/* Firstly, register our timer callback. */
	interrupt_install_handler(PIT_IRQ, &timer_callback);

	/* The value we send to the PIT is the value to divide it's input clock */
	/* (1193180 Hz) by, to get our required frequency. */
	
	uint32_t divisor = (1193180) / frequency;
	//uint32_t divisor = (1193180) / frequency;

	/* Send the command byte. */
	outportb(0x43, 0x36);

	/* Divisor has to be sent byte-wise, so split here into upper/lower bytes. */
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

	/* Send the frequency divisor. */
	outportb(0x40, l);
	outportb(0x40, h);

	/* The clock starts with the timer */
	tsc_start = tsc_present ? rdtsc() : 0;

	dbgprintf("PIT initialized.\n");
}