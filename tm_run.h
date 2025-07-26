#ifndef TM_RUN_H
#define TM_RUN_H

/*
 * Represents a given run of a TM, which contains a reference to the transition
 * table, as well as the two tape representations we use (note that one of the tapes may be NULL
 * if we only wish to use one tape representation).
 */
struct tm_run_t {
	// NOTE that the transition table is just a reference and not managed by this struct!
	const struct tm_def_t *def;	// transition table (reference)

	// these variables "belong" to the run and are allocated and freed, etc.
	struct rle_tape_t *rle_tape;		// tape (RLE representation)
	struct flat_tape_t *flat_tape;		// tape (flat representation)

	int steps;							// the number of steps performed
	int state;							// the current state
	int prev_delta;						// last direction moved, initially zero, then always -1 or 1
	// invariants: compare_rle_flat_tapes(rle_tape, flat_tape) == 0 given that both are non-null.
};


struct tm_run_t *tm_run_init(const struct tm_def_t *const def, const int use_rle_tape, const int flat_tape_len, const int flat_tape_off);
void tm_run_free(struct tm_run_t *const run);
int tm_run_halted(const struct tm_run_t *const run);
int tm_run_step(struct tm_run_t *run);
int tm_run_steps(struct tm_run_t *run, int max_steps);
void tm_run_print_tape(const struct tm_run_t *const run, const int directed);

#endif
