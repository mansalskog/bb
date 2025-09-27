#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tm_com.h"
#include "util.h"

#include "tape_flat.h"

/*
 * A flat tape that automatically expands (by doubling the size) if we run off the edge.
 */
struct flat_tape_t {
	sym_t *syms;		// symbol memory array
	int len;			// length of the full tape
	int rel_pos;		// relative position (0 = TM starting position)
	int init_pos;		// initial position, to ensure mem_pos := rel_pos + init_pos is always within the array
	unsigned sym_bits;	// the width of one symbol in bits (must be <= MAX_SYM_BITS)
	// invariants: 0 <= rel_pos + init_pos < len
};

/*
 * Frees the memory used by a flat tape.
 */
void flat_tape_free(struct flat_tape_t *const tape)
{
	free(tape->syms);
	free(tape);
}

/*
 * Creates a flat tape of the specified initial size filled with zeros.
 * The absolute position is initialized to zero.
 */
struct flat_tape_t *flat_tape_init(const int len, const int init_pos, const unsigned sym_bits)
{
	assert(0 <= init_pos && init_pos < len);			// This ensures that our intial position is within the syms array
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct flat_tape_t *const tape = malloc(sizeof *tape);
	tape->syms = malloc((size_t) len * sizeof *tape->syms);
	tape->len = len;
	tape->rel_pos = 0;
	tape->init_pos = init_pos;
	tape->sym_bits = sym_bits;

	return tape;
}

/*
 * Prints an excerpt of the flat tape with a given ctx number of symbols
 * on either side of the head.
 */
void flat_tape_print(const struct flat_tape_t *const tape, const int ctx, const state_t state, const int directed)
{
	const int mem_pos = tape->rel_pos + tape->init_pos;
	// The left limit, may be less than 0
	const int left = mem_pos - ctx;
	// Number of symbols outside the tape to print (distance from 0th sym)
	const int left_out = maximum(0 - left, 0);
	assert(left + left_out >= 0);

	// The right limit, may be larger than len - 1
	const int right = mem_pos + ctx + 1;
	// Number of symbols outside the tape to print (distance from last sym)
	const int right_out = maximum(right - (tape->len - 1), 0);
	assert(right - right_out < tape->len);

	// Tape before head
	for (int i = 0; i < left_out; i++)
		printf(".");
	for (int i = left + left_out; i < mem_pos; i++) {
		sym_bin_print(tape->syms[i], tape->sym_bits);
		printf(" ");
	}

	// Head and current state
	print_state_and_sym(state, tape->syms[mem_pos], tape->sym_bits, directed);
	printf(" ");

	// Tape after head
	for (int i = mem_pos + 1; i <= right - right_out; i++) {
		sym_bin_print(tape->syms[i], tape->sym_bits);
		printf(" ");
	}

	for (int i = 0; i < right_out; i++)
		printf(".");
	printf("\n");
}

/*
 * Moves left or right in the flat tape, reallocating tape as required if we move outside.
 * NOTE: it is not very efficient to allocate 100% more memory just because we went outside
 * on one side by one symbol. Perhaps we should be later change to only add 50% more on the
 * side we actually went outside. Depends on the statistical properties of TMs, e.g.
 * "bouncers" (allocate both sides is good) vs "translated cycles" (allocate one side is good), etc.
 */
void flat_tape_move(struct flat_tape_t *const tape, int delta)
{
	// This defines the min/max allowed tape head positions, namely INT_MIN+1 and INT_MAX-1
	assert(delta == -1 || delta == 1);
	if (delta == -1)
		assert(INT_MIN + 2 < tape->rel_pos && tape->rel_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < tape->rel_pos && tape->rel_pos < INT_MAX - 2);

	const int mem_pos = tape->rel_pos + tape->init_pos;
	if (mem_pos + delta < 0 || mem_pos + delta >= tape->len) {
		// Ran out of tape, allocate more
		const int old_len = tape->len;
		const int new_len = old_len * 2;
		sym_t *const new_syms = malloc(sizeof *new_syms * (size_t) new_len);

		/* Place data in the "middle" of the tape, growing it by about 50% on both sides
		 * We need to increment the offset by half of the old length, which may be one unit
		 * "too little" if the old_len was not divisible by two, but as long as we copy
		 * everything correctly, this does not matter, as next time the tape will move by
		 * exactly half of new_len.
		 */
		const int offset = old_len / 2;
		memset(new_syms, 0, (size_t) offset * sizeof *new_syms);
		memcpy(new_syms + (ptrdiff_t) offset, tape->syms, (size_t) old_len * sizeof *new_syms);
		memset(new_syms + (ptrdiff_t) (offset + old_len), 0, (size_t) (new_len - old_len - offset) * sizeof *new_syms);

		tape->len = new_len;
		free(tape->syms);
		tape->syms = new_syms;
		tape->init_pos += offset;
	}

	tape->rel_pos += delta;
	assert(0 <= tape->rel_pos + tape->init_pos && tape->rel_pos + tape->init_pos < tape->len);
}

/*
 * Writes a symbol to the tape.
 */
void flat_tape_write(const struct flat_tape_t *const tape, const sym_t sym)
{
	tape->syms[tape->rel_pos + tape->init_pos] = sym;
}

/*
 * Reads a symbol from the tape.
 */
sym_t flat_tape_read(const struct flat_tape_t *const tape)
{
	return tape->syms[tape->rel_pos + tape->init_pos];
}

/*
 * Counts the number of nonzero symbols in a flat tape, for use in e.g. the BB sigma function.
 * This requires a full scan of the tape. We could perhapst add variables for the leftmost and
 * rightmost position visited, but this is probably really fast.
 */
int flat_tape_count_nonzero(const struct flat_tape_t *const tape) {
	int nonzero = 0;
	for (int i = 0; i < tape->len; i++) {
		if (tape->syms[i] != 0)
			nonzero++;
	}
	return nonzero;
}
