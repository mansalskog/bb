// Dynamically allocated flat tape
#ifndef TM_TAPE_FLAT_H
#define TM_TAPE_FLAT_H

#include "tm_com.h"

struct flat_tape_t;

struct flat_tape_t *flat_tape_init(int len, int init_pos, unsigned sym_bits);

void flat_tape_free(struct flat_tape_t *tape);
sym_t flat_tape_read(const struct flat_tape_t *tape);
void flat_tape_write(const struct flat_tape_t *tape, const sym_t sym);
void flat_tape_move(struct flat_tape_t *tape, int delta);

void flat_tape_print(const struct flat_tape_t *tape, int ctx, state_t state, int directed);

#endif
