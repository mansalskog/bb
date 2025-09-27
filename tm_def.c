#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tm_com.h"
#include "tm_def.h"

/*
 * Stores a given instruction in the transition table.
 */
static void tm_def_store(struct tm_def_t *const def, const state_t state, const sym_t sym, const struct tm_instr_t instr)
{
	assert(0 <= state && state < def->n_states);
	assert(0 <= sym && sym < def->n_syms);

	// assert(0 <= instr.state && instr.state < def->n_states); // TODO allow for 0xFF state etc.
	assert(0 <= instr.sym && instr.sym < def->n_syms);
	assert(instr.dir == DIR_LEFT || instr.dir == DIR_RIGHT);

	const int idx = state * def->n_syms + sym;
	def->instr_tab[idx] = instr;
}

/*
 * Retrieves an instruction from the transition table.
 */
struct tm_instr_t tm_def_lookup(const struct tm_def_t *const def, const state_t state, const sym_t sym)
{
	assert(0 <= state && state < def->n_states);
	assert(0 <= sym && sym < def->n_syms);

	const int idx = state * def->n_syms + sym;
	return def->instr_tab[idx];
}

/*
 * Allocates an empty transition table of the specified size. We must fill it with states
 * before running.
 */
static struct tm_def_t *tm_def_init(const int n_syms, const int n_states)
{
	assert(n_syms > 0 && n_states > 0);
	const size_t tab_size = (size_t) (n_syms * n_states);
	struct tm_def_t *def = malloc(sizeof *def + tab_size * sizeof *def->instr_tab);
	def->n_syms = n_syms;
	def->n_states = n_states;
	return def;
}

/*
 * Frees a TM definition (transition table).
 */
void tm_def_free(struct tm_def_t *const def)
{
	free(def);
}

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')

/* Parses a Turing Machine from a given standard text format
 * e.g. "1RB1LB_1LA1LZ". This is a bit ugly to deal with
 * possible input format errors...
 */
struct tm_def_t *tm_def_parse(const char *const txt)
{
	// Find first underscore and thus number of columns
	int cols = 0;
	while(txt[cols] != 0 && txt[cols] != '_')
		cols++;

	if (cols % 3 != 0) {
		ERROR("Invalid width %d of row, should be divisible by 3.\n", cols);
	}
	const int n_syms = cols / 3;

	const size_t len = strlen(txt);
	// Each row contains three characters per sym, and one underscode for each except the last row
	// We check that the string is well-formed below, so no length check here
	// Also this cast is OK because the number of states should fit in an int
	const int n_states = ((int) len + 1) / (n_syms * 3 + 1);

	struct tm_def_t *def = tm_def_init(n_syms, n_states);

	for (state_t i_state = 0; i_state < def->n_states; i_state++) {
		for (sym_t i_sym = 0; i_sym < def->n_syms; i_sym++) {
			const int txt_idx = i_state * (def->n_syms * 3 + 1) + i_sym * 3; // base index into txt

			const char sym_c = txt[txt_idx];
			if (!sym_c || sym_c < '0' || sym_c - '0' >= def->n_syms) {
				// Allow unused states only if all three chars are '-'
				// NB. short-circuiting prevents reading past end of string here
				if (sym_c == '-' && txt[txt_idx + 1] == '-' && txt[txt_idx + 2] == '-') {
					const struct tm_instr_t instr = {
						0, 				// dummy, will not be read
						STATE_UNDEF,	// halting state
						DIR_LEFT, 		// dummy, will not be read
					};
					tm_def_store(def, i_state, i_sym, instr);
					continue;
				}
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym_c), i_state, i_sym, '0' + def->n_syms - 1);
			}

			const char dir_c = txt[txt_idx + 1];
			if (!dir_c || (dir_c != 'L' && dir_c != 'R')) {
				ERROR("Invalid direction %c at row %d col %d.\n",
					ONLY_ALNUM(dir_c), i_state, i_sym);
			}

			const char state_c = txt[txt_idx + 2];
			if (!state_c || state_c < 'A' || state_c - 'A' >= def->n_states) {
				if (state_c != 'Z' && state_c != 'H') {
					WARN("Unusual halting state state %c at row %d col %d, should be either A-%c or H or Z.\n",
						ONLY_ALNUM(state_c), i_state, i_sym, 'A' + def->n_states - 1);
				}
			}
			// Save the instruction
			const struct tm_instr_t instr = {
				(sym_t) (sym_c - '0'),
				(state_t) (state_c - 'A'),
				dir_c == 'L' ? DIR_LEFT : DIR_RIGHT,
			};
			tm_def_store(def, i_state, i_sym, instr);
		}

		// Read an underscore as terminator (just to verify input format)
		const char term = txt[i_state * (def->n_syms * 3 + 1) + def->n_syms * 3];
		// NB after the last row we expect a null char, otherwise an '_'
		if (i_state < def->n_states - 1 && term != '_') {
			printf("%d\n", term);
			ERROR("Invalid row terminator %c at row %d, should be underscore.\n",
				ONLY_ALNUM(term), i_state);
		}
		if (i_state == def->n_states - 1 && term != 0) {
			ERROR("Trailing character %c at row %d, expected end of input.\n",
				ONLY_ALNUM(term), i_state);
		}
	}

	return def;
}

/*
 * Prints the "program" of the given TM as a table.
 * We may optionally print the states as "directed" which means they are displayed as
 * "<A", ">A", "<B", ">B", ... instead of "A", "B", "C", "D", ...
 */
void tm_def_print(const struct tm_def_t *const def, const int directed)
{
	printf(" ");
	for (int i_sym = 0; i_sym < def->n_syms; i_sym++) {
		printf(" %d  ", i_sym + 1);
	}
	for (state_t i_state = 0; i_state < def->n_states; i_state++) {
		printf("\n%c ", 'A' + i_state);
		for (sym_t i_sym = 0; i_sym < def->n_syms; i_sym++) {
			struct tm_instr_t instr = tm_def_lookup(def, i_state, i_sym);
			printf("%d%c", // NB we use decimal here but binary in tape...
				instr.sym,
				instr.dir == DIR_LEFT ? 'L' : 'R');
			if (directed) {
				printf("%c%c ", 'A' + (i_state >> 1U), (i_state & 1U) == DIR_LEFT ? '<' : '>');
			} else {
				printf("%c ", 'A' + i_state);
			}
		}
	}
	printf("\n");
}
