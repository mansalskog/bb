#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "tm_com.h"

/*
 * Prints a symbol as a string of binary digits to stdout.
 * The width is fixed (leading zeros included).
 */
void sym_bin_print(const sym_t sym, const unsigned sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);
	for (int i = (int) sym_bits - 1; i >= 0; i--) {
		// Don't bother shifting down because we only care if zero or not
		const unsigned bit = sym & (1U << i);
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
