#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tape_bit.h"
#include "util.h"

typedef unsigned long bit_block_t;

#define BLOCK_BITS (CHAR_BIT * sizeof (bit_block_t))

/*
 * Contains an array of "syms" each consisting of sym_bits bits. They are stored packed in
 * "units".
 */
struct bit_tape_t {
	int n_syms;				// number of symbols in the tape
	unsigned sym_bits;		// number of bits per symbol
	int rel_pos;			// relative position (0 = TM starting position)
	int init_pos;			// initial position, to ensure mem_pos := rel_pos + init_pos is always within the array
	bit_block_t blocks[];	// this should always point to an array of n_blocks_for(n_syms, sym_bits) blocks
};

/*
 * The number of units required to store n_syms of sym_bits each is the product divided
 * by BLOCK_BITS, rounded up (achieved by adding BLOCK_BITS - 1 before division).
 */
static unsigned n_blocks_for(const int n_syms, const unsigned sym_bits)
{
	// FIXME: adding BLOCK_BITS - 1 results in a buffer overflow, investigate
	return ((unsigned) n_syms * sym_bits + BLOCK_BITS) / BLOCK_BITS;
}

struct tape_t *bit_tape_init(const unsigned sym_bits, const int n_syms, const int init_pos)
{
	// Allocate the data blocks together with the metadata struct and zero them out with a memset
	const size_t n_blocks = (size_t) n_blocks_for(n_syms, sym_bits);
	struct bit_tape_t *const data = malloc(sizeof *data + n_blocks * sizeof *data->blocks);
	memset(data->blocks, 0, n_blocks * sizeof *data->blocks);
	data->n_syms = n_syms;
	data->sym_bits = sym_bits;
	data->rel_pos = 0;
	data->init_pos = init_pos;

	struct tape_t *const tape = malloc(sizeof *tape);
	tape->data = data;
	tape->free = bit_tape_free;
	tape->write = bit_tape_write;
	tape->read = bit_tape_read;
	tape->move = bit_tape_move;
	return tape;
}

void bit_tape_free(struct tape_t *const tape)
{
	struct bit_tape_t *const data = tape->data;
    free(data);
	free(tape);
}

sym_t bit_tape_read(const struct tape_t *const tape)
{
	const struct bit_tape_t *const data = tape->data;

	const int sym_idx = data->init_pos + data->rel_pos;
	// Check that we're reading within the tape
	assert(0 <= sym_idx && sym_idx < data->n_syms);

	const unsigned bit_from = (unsigned) sym_idx * data->sym_bits;
	const unsigned bit_to = ((unsigned) sym_idx + 1) * data->sym_bits;
	const unsigned unit_from = bit_from / BLOCK_BITS;
	const unsigned unit_to = bit_to / BLOCK_BITS;
	// Check that we don't read outside out allocated memory
	assert(0 <= unit_from && unit_to < n_blocks_for(data->n_syms, data->sym_bits));

	const unsigned shift_low = bit_from % BLOCK_BITS;
    const unsigned shift_high = bit_to % BLOCK_BITS;

	if (unit_from == unit_to) {
		// Only required to read one unit
		const bit_block_t mask = bitmask(shift_low, bit_to % BLOCK_BITS);
		const bit_block_t unit = (data->blocks[unit_from] & mask) >> shift_low;
		return (sym_t) unit;
	}

	const bit_block_t low_mask = bitmask(shift_low, BLOCK_BITS);
	const bit_block_t low_unit = (data->blocks[unit_from] & low_mask) >> shift_low;
	const bit_block_t high_mask = bitmask(0, bit_to % BLOCK_BITS);
	const bit_block_t high_unit = (data->blocks[unit_to] & high_mask) << (data->sym_bits - shift_high);
	assert((low_unit & high_unit) == 0UL);
	return (sym_t) (low_unit | high_unit);
}

void bit_tape_write(struct tape_t *const tape, const sym_t sym)
{
	struct bit_tape_t *const data = tape->data;

	const int sym_idx = data->init_pos + data->rel_pos;
	// Check that we're writing within the tape
	assert(0 <= sym_idx && sym_idx < data->n_syms);

	// The symbol cannot contain more than the allowed bits
	assert((sym & ~bitmask(0, data->sym_bits)) == 0UL);

	const unsigned bit_from = (unsigned) sym_idx * data->sym_bits;
	const unsigned bit_to = ((unsigned) sym_idx + 1) * data->sym_bits;
	const unsigned unit_from = bit_from / BLOCK_BITS;
	const unsigned unit_to = bit_to / BLOCK_BITS;
	// Check that we don't write outside out allocated memory
	assert(0 <= unit_from && unit_to < n_blocks_for(data->n_syms, data->sym_bits));

	const unsigned shift_low = bit_from % BLOCK_BITS;
    const unsigned shift_high = bit_to % BLOCK_BITS;

	if (unit_from == unit_to) {
		// Only required to write one unit
		const bit_block_t mask = bitmask(shift_low, bit_to % BLOCK_BITS);
		const bit_block_t unit = (data->blocks[unit_from] & ~mask) | ((bit_block_t) sym << shift_low);
		data->blocks[unit_from] = unit;
		return;
	}

    // Write to two adjacent units
	const bit_block_t sym_mask = bitmask(0, BLOCK_BITS - shift_low);
	const bit_block_t low_mask = bitmask(shift_low, BLOCK_BITS);
	const bit_block_t high_mask = bitmask(0, shift_high);
	const bit_block_t high_unit = (data->blocks[unit_from] & ~low_mask) | (((bit_block_t) sym & sym_mask) << shift_low);
	const bit_block_t low_unit = (data->blocks[unit_to] & ~high_mask) | (((bit_block_t) sym & ~sym_mask) >> (data->sym_bits - shift_high));
	data->blocks[unit_from] = high_unit;
	data->blocks[unit_to] = low_unit;
}

void bit_tape_move(struct tape_t *const tape, int delta)
{
	struct bit_tape_t *data = tape->data;

	assert(delta == -1 || delta == 1);
	// TODO refactor to make position handling in tm_run instead?
	data->rel_pos += delta;
	if (!(0 <= data->rel_pos + data->init_pos && data->rel_pos + data->init_pos < data->n_syms)) {
		ERROR("Tried to move to %d but tape length is %d\n", data->rel_pos + data->init_pos, data->n_syms);
	}
}

static void test_basic(void)
{
	assert(BLOCK_BITS == 64);

	assert(n_blocks_for(10, 7) == 2);

	assert(bitmask(1, 4) == 14);
}

/*
 * Generates a pseudorandom symbol that's consistent for a given index.
 */
static sym_t prng_sym(int i, unsigned sym_bits)
{
    const unsigned seed = 12345678U;
    srand(seed * (unsigned) i);
    const bit_block_t mask = bitmask(0U, sym_bits);
    const bit_block_t unit = (((bit_block_t) rand() << (CHAR_BIT * sizeof (int))) | (bit_block_t) rand()) & mask;
	return (sym_t) unit;
}

static void test_write_and_read(void)
{
    const int n_syms = 1234;
    for (unsigned sym_bits = 1; sym_bits < BLOCK_BITS; sym_bits++) {
        struct tape_t *tape = bit_tape_init(sym_bits, n_syms, 0);

        for (int i = 0; i < n_syms; i++) {
            bit_tape_write(tape, prng_sym(i, sym_bits));
			if (i < n_syms - 1) {
				bit_tape_move(tape, 1);
			}
        }

        for (int i = n_syms - 1; i >= 0; i--) {
            bit_block_t written = prng_sym(i, sym_bits);
            bit_block_t read = bit_tape_read(tape);
			if (read != written) {
				ERROR("Expected read %lu to match written %lu\n", read, written);
			}
			if (i > 0) {
				bit_tape_move(tape, -1);
			}
        }

        bit_tape_free(tape);
    }
}

void bit_tape_test(void)
{
	test_basic();
    test_write_and_read();
}
