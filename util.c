#include <assert.h>
#include <limits.h>
#include <stdio.h>

// TODO clean up file structure
#include "tape.h"

#include "util.h"

/*
 * Computes the number of seconds between two timestamps.
 */
double seconds(const clock_t t1, const clock_t t0)
{
	return ((double) (t1 - t0)) / CLOCKS_PER_SEC;
}


/*
 * The maximum of two integers, used for clamping values etc.
 */
int maximum(const int a, const int b)
{
	return a > b ? a : b;
}

/*
 * Calculates how many bits are needed to fit a value, e.g. floor_log2(7) = 3.
 * Mathematically, this is equivalent to floor(log2(n)) or
 * the number m such that n >> (m + 1) == 0.
 */
unsigned ceil_log2(const unsigned n)
{
	assert(n >= 0); // Only valid for non-negative integers
	unsigned shift = 0;
	while ((unsigned) n >> shift > 0)
		shift++;
	return shift;
}

/*
 * Constructs a bitmask with ones in the specified positions. E.g. from = 1, to = 4 results
 * in mask = 14 = 0b1110.
 */
unsigned long bitmask(const unsigned from, const unsigned to)
{
	assert(from >= 0);
	assert(to >= 0);
	const unsigned long mask_low = (1UL << (to - from)) - 1UL;
	return mask_low << from;
}

/*
 * Prints a number as binary. Note that the lowest bit is the leftmost, to be consistent
 * with how arrays are displayed.
 */
void print_bin(const char *const pre, unsigned long val)
{
	if (pre) {
		printf("%s: ", pre);
	}
	do {
		printf("%lu", val & 1UL);
		val >>= 1UL;
	} while (val);
	if (pre) {
		printf("\n");
	}
}

#ifndef NDEBUG
void assert_eq(unsigned long a, unsigned long b)
{
    if (a != b) {
        printf("Expected equality, got %lu != %lu\n", a, b);
        assert(0);
    }
}
#endif

/*
 * Prints a symbol as a string of binary digits to stdout.
 * The width is fixed (leading zeros included).
 */
void sym_bin_print(const sym_t sym, const unsigned sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);
	for (int i = (int) sym_bits - 1; i >= 0; i--) {
		// Don't bother shifting down because we only care if zero or not
		const unsigned bit = sym & (1U << (unsigned) i);
		printf("%c", bit ? '1' : '0');
	}
}

/*
 * Prints a state as a letter from A to Z. This means we only
 * support up to 26 states. We probably don't need more, but
 * if we do, we have to invent a format for them.
 * Can print both with "directed" formatting and without
 */
void print_state_and_sym(
		const state_t state,
		const sym_t sym,
		const unsigned sym_bits,
		const int directed)
{
	if (directed) {
		dir_t dir = state & 1U;
		const state_t state_idx = state >> 1U;
		
		if (state_idx > 'Z' - 'A') {
			ERROR(
				"Can currently only print states A-Z (0-%d), tried to print %d.",
				'Z' - 'A',
				state_idx);
		}

		if (dir == DIR_LEFT) {
			sym_bin_print(sym, sym_bits);
			printf("<%c", 'A' + state_idx);
		} else {
			printf("%c>", 'A' + state_idx);
			sym_bin_print(sym, sym_bits);
		}
	} else {
		if (state > 'Z' - 'A') {
			ERROR(
				"Can currently only print states A-Z (0-%d), tried to print %d.",
				'Z' - 'A',
				state);
		}

		printf("[");
		sym_bin_print(sym, sym_bits);
		printf("]%c", 'A' + state);
	}
}
