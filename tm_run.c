#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tape.h"
#include "tm_def.h"
#include "util.h"

#include "tm_run.h"

static void instr_print(const struct tm_instr_t *const instr, const unsigned sym_bits, const int directed) {
	print_state_and_sym(instr->state, instr->sym, sym_bits, directed);
	printf("%c\n", instr->dir == DIR_LEFT ? 'L' : 'R');
}

/*
 * Frees the tape memory used by a certain TM run.
 * NOTE THIS DOES NOT FREE THE tm_def_t TRANSITION TABLE or TAPES
 * (because we might want to reuse them it for another run)
 */
void tm_run_free(struct tm_run_t *run)
{
	free(run);
}

/*
 * Initializes a new run (tape(s)) etc. for a given TM program.
 * We can specify if the machine should use a RLE tape or not.
 * We can also specify the initial length of the flat tape. This must be a (strictly) positive.
 * A non-positive value means that the flat tape is not enabled for this run.
 * We can also specify the flat tape offset. This is simply ignore if we have no flat tape.
 */
struct tm_run_t *tm_run_init(
		const struct tm_def_t *const def,
		struct tape_t *const tape1,
		struct tape_t *const tape2,
		struct tape_t *const tape3)
{
	if (!(tape1 || tape2 || tape3)) {
		ERROR("Must use at least one tape!\n");
	}

	struct tm_run_t *run = malloc(sizeof *run);
	run->def = def;
	run->tapes[0] = tape1;
	run->tapes[1] = tape2;
	run->tapes[2] = tape3;
	run->steps = 0;
	run->state = 0;	// always start in state 0, or 'A'
	return run;
}

/*
 * Checks whether the machine has halted (i.e. reached an undefined state). Returns 1 if halted,
 * 0 otherwise.
 */
int tm_run_halted(const struct tm_run_t *const run) {
	return run->state >= run->def->n_states;
}

/*
 * Runs one step for the machine. Return value is 1 if the machine halted, 0 otherwise.
 */
int tm_run_step(struct tm_run_t *const run)
{
	if (tm_run_halted(run)) {
		ERROR("Trying to step halted TM.\n");
	}

	const state_t i_state = run->state;
	state_t o_state = 0;
	for (int i = 0; i < MAX_TAPES; i++) {
		struct tape_t *tape = run->tapes[i];
		if (!tape)
			continue;

		// Read the symbol
		const sym_t i_sym = tape->read(tape);

		// Lookup the instruction
		struct tm_instr_t instr = tm_def_lookup(run->def, i_state, i_sym);

		// Write
		tape->write(tape, instr.sym);

		// Move
		int delta = instr.dir == DIR_LEFT ? -1 : 1;
		tape->move(tape, delta);

		o_state = instr.state;
	}

	// Update step and state
	run->state = o_state;
	run->steps++;

	return run->state >= run->def->n_states;
}

/*
 * Runs the machine until the given number of steps have passed, or
 * the machine has halted. Returns 1 on halt and 0 otherwise.
 * Note that max_steps is not cumulative (run->steps) but rather counts from the first step
 * taken by the current invocation of this function.
 */
int tm_run_steps(struct tm_run_t *const run, const int max_steps)
{
	int steps = 0;
	while (steps < max_steps) {
		int halt = tm_run_step(run);
		if (halt)
			return 1;
	}
	return 0;
}
