#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "util.h"

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')

void table_store(struct table_t *const table, const int i_state, const int i_sym, const struct action_t action)
{
	// Pack the struct into a bitfield and store into the tape
	const unit_t action_bits = (action.o_state << (ceil_log2(table->n_syms) + 1)) | (action.o_sym << 1) | action.o_dir;
	// TODO: better to store row-wise or col-wise?
	const int idx = i_state * table->n_syms + i_sym; 
	tape_write(&table->tape, idx, action_bits);
}

struct action_t table_lookup(const struct table_t *const table, const int i_state, const int i_sym)
{
	const int idx = i_state * table->n_syms + i_sym; // TODO: better to store row-wise or col-wise?
	const unit_t action_bits = tape_read(&table->tape, idx);
	// I presume that CPU is faster than memory, which is why I recompute these masks every time
	// rather than storing them in struct table_t for instance. Likewise I expect the optimizer
	// to optimize out these temporary variables so they don't pollute the cache too much.
	const unsigned sym_bits = ceil_log2(table->n_syms);
	const unsigned state_bits = ceil_log2(table->n_states + 1);
	const unit_t sym_mask = bitmask(1, sym_bits + 1);
	const unit_t state_mask = bitmask(sym_bits + 1, sym_bits + state_bits + 1);
	struct action_t action = {
		(action_bits & state_mask) >> (sym_bits + 1),
		(action_bits & sym_mask) >> 1,
		action_bits & 1UL,
	};
	return action;
}

/* Parses a Turing Machine from a given standard text format
 * e.g. "1RB1LB_1LA1LZ". This is a bit ugly to deal with
 * possible input format errors...
 */
struct table_t table_parse(const char *const txt)
{
	// Find first underscore and thus number of columns
	int cols = 0;
	while(txt[cols] != 0 && txt[cols] != '_')
		cols++;

	if (cols % 3 != 0) {
		ERROR("Invalid width %d of row, should be divisible by 3.\n", cols);
	}

	struct table_t table;
	table.n_syms = cols / 3;

	const size_t len = strlen(txt);
	// Each row contains three characters per sym, and one underscode for each except the last row
	// We check that the string is well-formed below, so no length check here
	// Also this cast is OK because the number of states should fit in an int
	table.n_states = (int) ((len + 1) / (table.n_syms * 3 + 1));

	// n_states does not include the halting state, which is represented as all ones
	const int state_bits = ceil_log2(table.n_states + 1);
	const unit_t state_halt = bitmask(0, state_bits);

	const int tape_n_syms = table.n_states * table.n_syms;
	const int tape_sym_bits = ceil_log2(table.n_states + 1) + ceil_log2(table.n_syms) + 1;
	table.tape = tape_init(tape_n_syms, tape_sym_bits);
	// Now we've initialized the table, so we're allowed to pass it to table_store (and table_lookup etc.)

	for (unsigned i_state = 0; i_state < table.n_states; i_state++) {
		for (unsigned i_sym = 0; i_sym < table.n_syms; i_sym++) {
			const int txt_idx = i_state * (table.n_syms * 3 + 1) + i_sym * 3; // base index into txt

			const char sym_c = txt[txt_idx];
			if (!sym_c || sym_c < '0' || sym_c > '9') {
				// Allow unused states only if all three chars are '-'
				// NB. short-circuiting prevents reading past end of string here
				if (sym_c == '-' && txt[txt_idx + 1] == '-' && txt[txt_idx + 2] == '-') {
					const struct action_t action = {
						state_halt,
						0, // dummy, will not be read
						DIR_RIGHT, // dummy, will not be read
					};
					table_store(&table, i_state, i_sym, action);
					
					continue;
				}
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym_c), i_state, i_sym, '0' + table.n_syms - 1);
			}
			const unsigned o_sym = sym_c - '0';
			if (o_sym >= table.n_syms) {
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym_c), i_state, i_sym, '0' + table.n_syms - 1);
			}

			const char dir_c = txt[txt_idx + 1];
			if (!dir_c || (dir_c != 'L' && dir_c != 'R')) {
				ERROR("Invalid direction %c at row %d col %d.\n",
					ONLY_ALNUM(dir_c), i_state, i_sym);
			}
			const unsigned o_dir = dir_c == 'R' ? DIR_RIGHT : DIR_LEFT;

			const char state_c = txt[txt_idx + 2];
			if (!state_c || state_c < 'A' || state_c > 'Z') {
				ERROR("Invalid state %c at row %d col %d, should be A-Z.\n",
					ONLY_ALNUM(state_c), i_state, i_sym);
			}
			unsigned o_state = state_c - 'A';
			if (o_state >= table.n_states) {
				o_state = state_halt;
				if (state_c != 'Z' && state_c != 'H')
					WARN("Unusual halting state state %c at row %d col %d, should be either A-%c or H or Z.\n",
						ONLY_ALNUM(state_c), i_state, i_sym, 'A' + table.n_states - 1);
			}
			// Save the instruction
			const struct action_t action = {
				o_state,
				o_sym,
				o_dir,
			};
			table_store(&table, i_state, i_sym, action);

#ifndef NDEBUG
			const struct action_t read_action = table_lookup(&table, i_state, i_sym);
			assert_eq(read_action.o_state, action.o_state);	
			assert_eq(read_action.o_sym, action.o_sym);	
			assert_eq(read_action.o_dir, action.o_dir);	
#endif
		}

		// Read an underscore as terminator (just to verify input format)
		const char term = txt[i_state * (table.n_syms * 3 + 1) + table.n_syms * 3];
		// NB after the last row we expect a null char, otherwise an '_'
		if (i_state < table.n_states - 1 && term != '_') {
			ERROR("Invalid row terminator %c at row %d, should be underscore.\n",
				ONLY_ALNUM(term), i_state);
		}
		if (i_state == table.n_states - 1 && term != 0) {
			ERROR("Trailing character %c at row %d, expected end of input.\n",
				ONLY_ALNUM(term), i_state);
		}
	}

	return table;
}
