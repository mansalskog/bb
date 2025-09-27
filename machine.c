#include "tape.h"
#include "table.h"
#include "machine.h"

struct machine_t machine_init(const struct table_t *const table, struct tape_t *const tape, unsigned start_pos)
{
	struct machine_t machine = {
		table,
		tape,
		0UL,
		start_pos,
		0,
	};
	return machine;
}

int machine_step(struct machine_t *const machine)
{
	if (machine->curr_state >= machine->table->n_states) {
		// Halted already
		return 1;
	}
	// Note: even if we halt in this step, it is counted.
	machine->curr_step++;
	const unit_t i_sym = tape_read(machine->tape, machine->curr_pos);
	struct action_t action = table_lookup(machine->table, machine->curr_state, i_sym);
	machine->curr_state = action.o_state;
	if (action.o_state >= machine->table->n_states) {
		// Halted
		return 1;
	}
	tape_write(machine->tape, machine->curr_pos, action.o_sym);
	machine->curr_pos += action.o_dir == DIR_RIGHT ? 1 : -1;
	return 0;
}

int machine_run(struct machine_t *const machine, const int max_steps)
{
	int steps = 0;
	while (1) {
		const int halted = machine_step(machine);
		if (halted)
			return 1;
		if (++steps >= max_steps)
			break;
	}
	return 0;
}
