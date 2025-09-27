#ifndef _TAPE_H
#define _TAPE_H

typedef unsigned long unit_t;

#define UNIT_BITS (CHAR_BIT * sizeof (unit_t))

/*
 * Contains an array of "syms" each consisting of sym_bits bits. They are stored packed in
 * "units".
 */
struct tape_t {
	unit_t *units;		// this should always point to an array of n_units(tape) units
	unsigned n_syms;			// number of symbols in the tape
	unsigned sym_bits;		// number of bits per symbol
};

struct tape_t tape_init(unsigned n_syms, unsigned sym_bits);
void tape_free(struct tape_t *tape);
unit_t tape_read(const struct tape_t *tape, unsigned sym_idx);
void tape_write(struct tape_t *tape, unsigned sym_idx, unit_t sym);
void tape_test();

#endif
