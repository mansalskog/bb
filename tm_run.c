#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tm_run.h"

///// Basic utility and output functions /////

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')

/*
 * Prints a symbol as a string of binary digits to stdout.
 * The width is fixed (leading zeros included).
 */
void sym_bin_print(const sym_t sym, const unsigned sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);
	for (int i = sym_bits - 1; i >= 0; i--) {
		// Don't bother shifting down because we only care if zero or not
		const unsigned bit = sym & (1U << i);
		printf("%c", bit ? '1' : '0');
	}
}

/*
 * Prints a state as a letter from A to Z. This means we only
 * support up to 26 states. We probably don't need more, but
 * if we do, we have to invent a format for them.
 * Can print both with "directed" formatting and without
 */
void print_state_and_sym(
		const state_t state,
		const sym_t sym,
		const unsigned sym_bits,
		const int directed)
{
	if (directed) {
		dir_t dir = state & 1U;
		const state_t state_idx = state >> 1U;
		
		if (state_idx > 'Z' - 'A') {
			ERROR(
				"Can currently only print states A-Z (0-%d), tried to print %d.",
				'Z' - 'A',
				state_idx);
		}

		if (dir == DIR_LEFT) {
			sym_bin_print(sym, sym_bits);
			printf("<%c", 'A' + state_idx);
		} else {
			printf("%c>", 'A' + state_idx);
			sym_bin_print(sym, sym_bits);
		}
	} else {
		if (state > 'Z' - 'A') {
			ERROR(
				"Can currently only print states A-Z (0-%d), tried to print %d.",
				'Z' - 'A',
				state);
		}

		printf("[");
		sym_bin_print(sym, sym_bits);
		printf("]%c", 'A' + state);
	}
}

void instr_print(const struct tm_instr_t *const instr, const unsigned sym_bits, const int directed) {
	print_state_and_sym(instr->state, instr->sym, sym_bits, directed);
	printf("%c\n", instr->dir == DIR_LEFT ? 'L' : 'R');
}

/*
 * Frees the tape memory used by a certain TM run.
 * NOTE THIS DOES NOT FREE THE tm_def_t TRANSITION TABLE!
 * (because we probably want to reuse it for another run)
 */
void tm_run_free(struct tm_run_t *run)
{
	if (run->rle_tape)
		rle_tape_free(run->rle_tape);
	if (run->flat_tape)
		flat_tape_free(run->flat_tape);
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
		const int use_rle_tape,
		const int flat_tape_len,
		const int flat_tape_off)
{
	if (!use_rle_tape && flat_tape_len <= 0) {
		ERROR("Must use at least one of RLE tape and flat tape!\n");
	}

	struct tm_run_t *run = malloc(sizeof *run);
	run->def = def;

	const unsigned sym_bits = floor_log2(def->n_syms);
	run->rle_tape = use_rle_tape ? rle_tape_init(sym_bits) : NULL;
	// NB we use initial length 2, but we could start with a larger len as a (small) optimization
	if (flat_tape_len <= 0)
		run->flat_tape = NULL;
	else
		run->flat_tape = flat_tape_init(flat_tape_len, flat_tape_off, sym_bits);

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

	// Read the symbol
	sym_t in_sym;
	if (run->rle_tape)
		in_sym = rle_tape_read(run->rle_tape);
	else if (run->flat_tape)
		in_sym = flat_tape_read(run->flat_tape);
	else
		ERROR("No tape for TM run!\n");

	// Lookup the instruction
	struct tm_instr_t instr = tm_def_lookup(run->def, run->state, in_sym);
	int delta = instr.dir == DIR_LEFT ? -1 : 1;

	// Flat tape write and move
	if (run->flat_tape) {
		flat_tape_write(run->flat_tape, instr.sym);
		flat_tape_move(run->flat_tape, delta);
	}

	// RLE tape write and move
	if (run->rle_tape) {
		rle_tape_write(run->rle_tape, instr.sym);
		rle_tape_move(run->rle_tape, delta);
	}

	// Update step, state and previous delta
	run->state = instr.state;
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

/*
 * Prints the tape(s) of the TM run.
 */
void tm_run_print_tape(const struct tm_run_t *const run, int directed)
{
	if (run->rle_tape) {
		rle_tape_print(
				run->rle_tape,
				run->state,
				directed);
	}

	if (run->flat_tape) {
		const int context = 5;
		flat_tape_print(
				run->flat_tape,
				context,
				run->state,
				directed);
	}
}
