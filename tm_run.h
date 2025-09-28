#ifndef TM_RUN_H
#define TM_RUN_H

#include "tape.h"
#include "tm_def.h"
#include "util.h"

#define MAX_TAPES 3

/*
 * Represents a given run of a TM, which contains a reference to the transition
 * table, as well as the two tape representations we use (note that one of the tapes may be NULL
 * if we only wish to use one tape representation).
 */
struct tm_run_t {
	// NOTE that the transition table is just a reference and not managed by this struct!
	const struct tm_def_t *def;	// transition table (reference)

	struct tape_t *tapes[MAX_TAPES];	// list of all the tapes to use, unused are set to NULL

	int steps;							// the number of steps performed
	state_t state;						// the current state
	int prev_delta;						// last direction moved, initially zero, then always -1 or 1
	// invariants: compare_rle_flat_tapes(rle_tape, flat_tape) == 0 given that both are non-null.
};


struct tm_run_t *tm_run_init(const struct tm_def_t *def, struct tape_t *tape1, struct tape_t *tape2, struct tape_t *tape3);
void tm_run_free(struct tm_run_t *run);
int tm_run_halted(const struct tm_run_t *run);
int tm_run_step(struct tm_run_t *run);
int tm_run_steps(struct tm_run_t *run, int max_steps);

#endif
