#ifndef TM_H
#define TM_H

#include <limits.h>

struct rle_tape_t;
struct flat_tape_t;
struct tm_def_t;

/* Represents a given run of a TM, which contains a reference to the transition
 * table.
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
	// invariants: rle_tape->abs_pos == flat_tape->abs_pos
};

///// Function declarations /////

struct tm_def_t *tm_def_parse(const char *const txt);
void tm_def_free(struct tm_def_t *const def);
void tm_def_print(const struct tm_def_t *const def);

struct tm_run_t *tm_run_init(const struct tm_def_t *const def);
void tm_run_free(struct tm_run_t *const run);
int tm_run_step(struct tm_run_t *run);
int tm_run_steps(struct tm_run_t *run, int max_steps);
void tm_run_print_tape(const struct tm_run_t *const run);

#endif
