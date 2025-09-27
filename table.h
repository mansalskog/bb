#ifndef _TABLE_H
#define _TABLE_H

#include <limits.h>

#include "tape.h"

#define DIR_RIGHT 0
#define DIR_LEFT 1

/**
 * Represents an action to be taken by a TM, with the next state, the written symbol,
 * and the direction. Used only to pass this to and from table_store and table_lookup, respectively.
 */
struct action_t {
	unsigned o_state;
	unsigned o_sym;
	unsigned o_dir;
};

/**
 * Defines a transition table for a turing machine as a "tape" (named table.next) that has
 * next.sym_bits = floor(log2(table.n_states + 1)) + floor(log2(table.n_syms)) + 1
 * i.e. each slot stores the next state, followed by the next sym, followed by the direction.
 * Note that table.n_states does not include the halting state, which must be added.
 * The table has next.n_syms = n_states * n_syms, i.e. one slot for each current state and read symbol.
 */
struct table_t {
	struct tape_t tape;
	unsigned n_states;
	unsigned n_syms;
};

void table_store(struct table_t *table, int i_state, int i_sym, struct action_t action);
struct action_t table_lookup(const struct table_t *table, int i_state, int i_sym);
struct table_t table_parse(const char *txt);

#endif
