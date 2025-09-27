#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned long unit_t;

const int UNIT_BITS = CHAR_BIT * sizeof (unit_t);

void assert_eq(unit_t a, unit_t b)
{
    if (a != b) {
        printf("Expected equality, got %lu != %lu\n", a, b);
        exit(1);
    }
}

/*
 * Contains an array of "syms" each consisting of sym_bits bits. They are stored packed in
 * "units".
 */
struct tape_t {
	unit_t *units;		// this should always point to an array of n_units(tape) units
	int n_syms;			// number of symbols in the tape
	int sym_bits;		// number of bits per symbol
};

/*
 */
void print_bin(const char *const pre, unit_t unit) {
	if (pre) {
		printf("%s: ", pre);
	}
	do {
		printf("%lu", unit & 1UL);
		unit >>= 1;
	} while (unit);
	if (pre) {
		printf("\n");
	}
}

/*
 * The number of units required to store n_syms of sym_bits each is the product divided
 * by UNIT_BITS, rounded up (achieved by adding UNIT_BITS - 1 before division).
 */
int n_units(const int n_syms, const int sym_bits)
{
	return (n_syms * sym_bits + UNIT_BITS - 1) / UNIT_BITS;
}

/*
 * Constructs a bitmask with ones in the specified positions. E.g. from = 1, to = 4 results
 * in mask = 14 = 0b1110.
 */
unit_t mask_bits(const int from, const int to)
{
	const unit_t mask_low = (1UL << (to - from)) - 1UL;
	return mask_low << from;
}

void tape_init(struct tape_t *const tape, const int n_syms, const int sym_bits)
{
	tape->units = calloc(n_units(n_syms, sym_bits), sizeof *tape->units);
	tape->n_syms = n_syms;
	tape->sym_bits = sym_bits;
}

void tape_free(struct tape_t *const tape) {
    free(tape->units);
}

unit_t tape_read(const struct tape_t *const tape, const int sym_idx)
{
	const int bit_from = sym_idx * tape->sym_bits;
	const int bit_to = (sym_idx + 1) * tape->sym_bits;
	const int unit_from = bit_from / UNIT_BITS;
	const int unit_to = bit_to / UNIT_BITS;
	const int shift_low = bit_from % UNIT_BITS;
    const int shift_high = bit_to % UNIT_BITS;

	if (unit_from == unit_to) {
		// Only required to read one unit
		const unit_t mask = mask_bits(shift_low, bit_to % UNIT_BITS);
		return (tape->units[unit_from] & mask) >> shift_low;
	}

	const unit_t low_mask = mask_bits(shift_low, UNIT_BITS);
	const unit_t low_unit = (tape->units[unit_from] & low_mask) >> shift_low;
	const unit_t high_mask = mask_bits(0, bit_to % UNIT_BITS);
	const unit_t high_unit = (tape->units[unit_to] & high_mask) << (tape->sym_bits - shift_high);
	assert((low_unit & high_unit) == 0UL);
	return low_unit | high_unit;
}

void tape_write(struct tape_t *const tape, int sym_idx, unit_t sym)
{
	// The symbol cannot contain more than the allowed bits
	assert((sym & ~mask_bits(0, tape->sym_bits)) == 0UL);

	const int bit_from = sym_idx * tape->sym_bits;
	const int bit_to = (sym_idx + 1) * tape->sym_bits;
	const int unit_from = bit_from / UNIT_BITS;
	const int unit_to = bit_to / UNIT_BITS;
	const int shift_low = bit_from % UNIT_BITS;
    const int shift_high = bit_to % UNIT_BITS;

	if (unit_from == unit_to) {
		// Only required to write one unit
		const unit_t mask = mask_bits(shift_low, bit_to % UNIT_BITS);
		tape->units[unit_from] = (tape->units[unit_from] & ~mask) | (sym << shift_low);
		return;
	}

    // Write to two adjacent units
	const unit_t sym_mask = mask_bits(0, UNIT_BITS - shift_low);
	const unit_t low_mask = mask_bits(shift_low, UNIT_BITS);
	const unit_t high_mask = mask_bits(0, shift_high);
	tape->units[unit_from] = (tape->units[unit_from] & ~low_mask) | ((sym & sym_mask) << shift_low);
	tape->units[unit_to] = (tape->units[unit_to] & ~high_mask) | ((sym & ~sym_mask) >> (tape->sym_bits - shift_high));
}

void test_basic()
{
	assert(UNIT_BITS == 64);

	assert(n_units(10, 7) == 2);

	assert(mask_bits(1, 4) == 14);
}

/*
 * Generates a pseudorandom symbol that's consisten for a given index.
 */
unit_t prng_sym(int i, int sym_bits)
{
    const int seed = 0x12345678;
    srand(seed * i);
    const unit_t mask = mask_bits(0, sym_bits);
    return (((unit_t) rand() << (CHAR_BIT * sizeof (int))) | rand()) & mask;
}

void test_write_and_read()
{
    const int n_syms = 1234;
    for (int sym_bits = 1; sym_bits < UNIT_BITS; sym_bits++) {
        struct tape_t tape;
        tape_init(&tape, n_syms, sym_bits);

        const time_t t = time(0);
        for (int i = 0; i < n_syms; i++) {
            tape_write(&tape, i, prng_sym(i, sym_bits));
        }

        for (int i = 0; i < n_syms; i++) {
            srand(t * i);
            unit_t written = prng_sym(i, sym_bits);
            unit_t read = tape_read(&tape, i);
            assert_eq(read, written);
        }

        tape_free(&tape);
    }
}

// TEMPORARY TEST
int main() {
	test_basic();
    test_write_and_read();
}


