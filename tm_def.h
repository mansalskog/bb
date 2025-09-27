#ifndef TM_DEF_H
#define TM_DEF_H

#include "tm_com.h"

/*
 * Represents an "instruction" in the transition table.
 */
struct tm_instr_t {
	sym_t sym;			// the symbol to write
	state_t state;		// the next state to enter
	dir_t dir;			// the move direction; it is only one bit
};

/*
 * A definition of a Turing Machine transition table, as a static structure.
 * The function is (in_state, in_sym) -> (out_state, out_sym, move_dir) where
 * idx = in_state * n_syms + in_sym
 * out_state = state_tab[idx]
 * out_sym = sym_tab[idx]
 * move_dir = (move_dirs[idx / DIR_TAB_BITS_PER_FIELD] >> (idx % DIR_TAB_BITS_PER_FIELD)) & 1;
 * Note that we can get sym_bits from a tm_def_t by sym_bits = ceil_log2(n_syms)
 */
struct tm_def_t {
	int n_syms;						// number of symbols
	int n_states;					// number of states
	struct tm_instr_t instr_tab[];	// symbol transition table
};

struct tm_def_t *tm_def_parse(const char *txt);
struct tm_instr_t tm_def_lookup(const struct tm_def_t *def, state_t state, sym_t sym);
void tm_def_print(const struct tm_def_t *def, int directed);
void tm_def_free(struct tm_def_t *def);

#endif
