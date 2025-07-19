#ifndef TM_H
#define TM_H

#include <limits.h>

///// Data declearations /////

struct rle_tape_t;
struct flat_tape_t;
struct tm_def_t;

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

///// Constant declarations /////

/*
 * Because we want to return the position where the tapes differ (i.e. the first one found, there
 * may be more differences), we need to use these sentinel values to represent equality or
 * difference in the actual position (that is, not difference AT a certain position).
 * Note that the minimum and maximum allowed positions are INT_MIN + 1 and INT_MAX - 1, respectively.
 */
#define TAPES_EQUAL INT_MIN
#define TAPES_DIFF_HEAD INT_MAX

///// Function declarations /////

struct tm_def_t *tm_def_parse(const char *const txt);
void tm_def_free(struct tm_def_t *const def);
void tm_def_print(const struct tm_def_t *const def);

struct tm_run_t *tm_run_init(const struct tm_def_t *const def);
void tm_run_free(struct tm_run_t *const run);
int tm_run_halted(const struct tm_run_t *const run);
int tm_run_step(struct tm_run_t *run);
int tm_run_steps(struct tm_run_t *run, int max_steps);
void tm_run_print_tape(const struct tm_run_t *const run);

int tm_flat_tape_cmp(const struct flat_tape_t *const tape_a, const struct flat_tape_t *const tape_b);
int tm_rle_tape_cmp(const struct rle_tape_t *const tape_a, const struct rle_tape_t *const tape_b);
int tm_mixed_tape_cmp(const struct rle_tape_t *const rle_tape, const struct flat_tape_t *const flat_tape);

#endif
