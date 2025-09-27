#ifndef _MACHINE_H
#define _MACHINE_H

#include "tape.h"
#include "table.h"

/**
 * Represents a running machine as pointers to its transition table, tape, and current
 * state, read/write head position and current step.
 */
struct machine_t {
	const struct table_t *table;
	struct tape_t *tape;
	unit_t curr_state;	
	unsigned curr_pos;
	unsigned curr_step;
};

struct machine_t machine_init(const struct table_t *table, struct tape_t *tape, unsigned start_pos);
int machine_step(struct machine_t *machine);
int machine_run(struct machine_t *machine, int max_steps);

#endif
