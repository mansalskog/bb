// Run Length Encoding tape
#ifndef TM_TAPE_RLE_H
#define TM_TAPE_RLE_H

#include "tape.h"
#include "util.h"

struct tape_t *rle_tape_init(unsigned sym_bits);

void rle_tape_free(struct tape_t *tape);
sym_t rle_tape_read(const struct tape_t *tape);
void rle_tape_write(struct tape_t *tape, sym_t sym);
void rle_tape_move(struct tape_t *tape, int delta);

struct rle_tape_t;
void rle_tape_print(const struct rle_tape_t *tape, state_t state, int directed);

#endif
