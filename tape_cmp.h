#ifndef TM_TAPE_CMP_H
#define TM_TAPE_CMP_H

int tm_flat_tape_cmp(const struct flat_tape_t *tape_a, const struct flat_tape_t *tape_b);
int tm_rle_tape_cmp(const struct rle_tape_t *tape_a, const struct rle_tape_t *tape_b);
int tm_mixed_tape_cmp(const struct rle_tape_t *rle_tape, const struct flat_tape_t *flat_tape);

#endif
