#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "tape_bit.h"
#include "util.h"

typedef unsigned long bit_block_t;

#define BIT_BLOCK_SIZE (CHAR_BIT * sizeof (bit_unit_t))

/*
 * Contains an array of "syms" each consisting of sym_bits bits. They are stored packed in
 * "units".
 */
struct bit_tape_t {
	int n_syms;				// number of symbols in the tape
	int sym_bits;			// number of bits per symbol
	int rel_pos;			// relative position (0 = TM starting position)
	int init_pos;			// initial position, to ensure mem_pos := rel_pos + init_pos is always within the array
	bit_block_t *blocks;	// this should always point to an array of n_units(tape) units
};

/*
 * The number of units required to store n_syms of sym_bits each is the product divided
 * by UNIT_BITS, rounded up (achieved by adding UNIT_BITS - 1 before division).
 */
static int n_blocks_for(const int n_syms, const int sym_bits)
{
	return (n_syms * sym_bits + UNIT_BITS - 1) / UNIT_BITS;
}

struct bit_tape_t *bit_tape_init(const int n_syms, const int sym_bits)
{
	const int n_blocks = n_blocks_for(n_syms, sym_bits);
	const struct bit_tape_t *const tape = malloc(sizeof *tape + (size_t) n_blocks * sizeof *tape->blocks);
	tape->n_syms = n_syms;
	tape->sym_bits = sym_bits;
	return tape;
}

void bit_tape_free(struct tape_t *const tape)
{
    free(tape->blocks);
	free(tape);
}

sym_t bit_tape_read(const struct bit_tape_t *const tape)
{
	const int sym_idx = tape->init_pos + tape->rel_pos;
	// Check that we're reading within the tape
	assert(0 <= sym_idx && sym_idx < tape->n_syms);

	const int bit_from = sym_idx * tape->sym_bits;
	const int bit_to = (sym_idx + 1) * tape->sym_bits;
	const int unit_from = bit_from / UNIT_BITS;
	const int unit_to = bit_to / UNIT_BITS;
	const int shift_low = bit_from % UNIT_BITS;
    const int shift_high = bit_to % UNIT_BITS;

	if (unit_from == unit_to) {
		// Only required to read one unit
		const unit_t mask = bitmask(shift_low, bit_to % UNIT_BITS);
		return (tape->units[unit_from] & mask) >> shift_low;
	}

	const unit_t low_mask = bitmask(shift_low, UNIT_BITS);
	const unit_t low_unit = (tape->units[unit_from] & low_mask) >> shift_low;
	const unit_t high_mask = bitmask(0, bit_to % UNIT_BITS);
	const unit_t high_unit = (tape->units[unit_to] & high_mask) << (tape->sym_bits - shift_high);
	assert((low_unit & high_unit) == 0UL);
	return low_unit | high_unit;
}

void bit_tape_write(struct bit_tape_t *const tape, const sym_t sym)
{
	const int sym_idx = tape->init_pos + tape->rel_pos;
	// Check that we're writing within the tape
	assert(0 <= sym_idx && sym_idx < tape->n_syms);

	// The symbol cannot contain more than the allowed bits
	assert((sym & ~bitmask(0, tape->sym_bits)) == 0UL);

	const int bit_from = sym_idx * tape->sym_bits;
	const int bit_to = (sym_idx + 1) * tape->sym_bits;
	const int unit_from = bit_from / UNIT_BITS;
	const int unit_to = bit_to / UNIT_BITS;
	const int shift_low = bit_from % UNIT_BITS;
    const int shift_high = bit_to % UNIT_BITS;

	if (unit_from == unit_to) {
		// Only required to write one unit
		const unit_t mask = bitmask(shift_low, bit_to % UNIT_BITS);
		tape->units[unit_from] = (tape->units[unit_from] & ~mask) | (sym << shift_low);
		return;
	}

    // Write to two adjacent units
	const unit_t sym_mask = bitmask(0, UNIT_BITS - shift_low);
	const unit_t low_mask = bitmask(shift_low, UNIT_BITS);
	const unit_t high_mask = bitmask(0, shift_high);
	tape->units[unit_from] = (tape->units[unit_from] & ~low_mask) | ((sym & sym_mask) << shift_low);
	tape->units[unit_to] = (tape->units[unit_to] & ~high_mask) | ((sym & ~sym_mask) >> (tape->sym_bits - shift_high));
}

void bit_tape_move(sturct bit_tape_t *const tape, int delta)
{
	assert(delta == -1 || delta == 1);
	// TODO refactor
	tape->rel_pos += delta;
	assert(0 <= tape->rel_pos + tape->init_pos && tape->rel_pos + tape->init_pos < tape->len);
}

static void test_basic(void)
{
	assert(UNIT_BITS == 64);

	assert(n_units(10, 7) == 2);

	assert(bitmask(1, 4) == 14);
}

/*
 * Generates a pseudorandom symbol that's consistent for a given index.
 */
static sym_t prng_sym(int i, int sym_bits)
{
    const int seed = 0x12345678;
    srand(seed * i);
    const unit_t mask = bitmask(0, sym_bits);
    return (((unit_t) rand() << (CHAR_BIT * sizeof (int))) | rand()) & mask;
}

static void test_write_and_read(void)
{
    const int n_syms = 1234;
    for (unsigned sym_bits = 1; sym_bits < UNIT_BITS; sym_bits++) {
        struct bit_tape_t tape = tape_init(n_syms, sym_bits);

        const time_t t = time(0);
        for (int i = 0; i < n_syms; i++) {
            tape_write(&tape, i, prng_sym(i, sym_bits));
        }

        for (int i = 0; i < n_syms; i++) {
            srand(t * i);
            unit_t written = prng_sym(i, sym_bits);
            unit_t read = tape_read(&tape, i);
			if (read != written) {
				ERROR("Expected read %lu to match written %lu\n", read, written);
			}
        }

        bit_tape_free(&tape);
    }
}

void bit_tape_test(void)
{
	test_basic();
    test_write_and_read();
}
