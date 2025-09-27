// Run Length Encoding tape
#ifndef TM_TAPE_RLE_H
#define TM_TAPE_RLE_H

#include "tm_com.h"

struct rle_tape_t;

struct rle_tape_t *rle_tape_init(unsigned sym_bits);

void rle_tape_free(struct rle_tape_t *tape);
sym_t rle_tape_read(const struct rle_tape_t *const tape);
void rle_tape_write(struct rle_tape_t *tape, sym_t sym);
void rle_tape_move(struct rle_tape_t *tape, int delta);

void rle_tape_print(const struct rle_tape_t *tape, state_t state, int directed);

#endif
