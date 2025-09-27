#include <assert.h>
#include <stdio.h>

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
unsigned long bitmask(const int from, const int to)
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
		val >>= 1;
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
