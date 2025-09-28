// Packed bit-field tape. Does NOT automatically grow, instead crashes with error
// Intended to be used where the programmer checks the bounds manually
#ifndef TM_BIT_TAPE_H
#define TM_BIT_TAPE_H

#include "tape.h"

struct tape_t *bit_tape_init(unsigned sym_bits, int n_syms, int init_pos);

void bit_tape_free(struct tape_t *tape);
sym_t bit_tape_read(const struct tape_t *tape);
void bit_tape_write(struct tape_t *tape, sym_t sym);
void bit_tape_move(struct tape_t *tape, int delta);

// Temporary, remove later
void bit_tape_test(void);

#endif
