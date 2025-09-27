// Packed bit-field tape. Does NOT automatically grow, instead crashes with error
// Intended to be used where the programmer checks the bounds manually
#ifndef _TAPE_H
#define _TAPE_H

struct bit_tape_t;

struct bit_tape_t *bit_tape_init(unsigned n_syms, unsigned sym_bits);

void bit_tape_free(struct tape_t *tape);
sym_t bit_tape_read(const struct bit_tape_t *tape);
void bit_tape_write(struct tape_t *tape, sym_t sym);
void bit_tape_move(struct bit_tape_t *tape, int delta);

#endif
