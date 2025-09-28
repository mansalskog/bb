// Dynamically allocated flat tape
#ifndef TM_TAPE_FLAT_H
#define TM_TAPE_FLAT_H

#include "tape.h"
#include "util.h"

struct tape_t *flat_tape_init(unsigned sym_bits, int len, int init_pos);

void flat_tape_free(struct tape_t *tape);
sym_t flat_tape_read(const struct tape_t *tape);
void flat_tape_write(struct tape_t *tape, sym_t sym);
void flat_tape_move(struct tape_t *tape, int delta);

struct flat_tape_t;
void flat_tape_print(const struct flat_tape_t *tape, int ctx, state_t state, int directed);

#endif
