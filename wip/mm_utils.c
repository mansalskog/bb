
/*
 * Determine one transition for t
 */
struct tm_instr_t mm_determine_instr(
		const struct tm_def_t *const tm_def,
		const int scale,
		const state_t mm_in_state,
		const sym_t mm_in_sym)
{
	const dir_t mm_in_dir = mm_in_state & 1;		// The direction is stored as the lowest bit of the macro state
	const state_t tm_in_state = mm_in_state >> 1;	// The initial state of our TM.
	/* We allocate two extra symbols so that we don't run off the edge, but these should never be
	 * written to. This means that the position rel_pos == 0 is the second symbol in the array,
	 * which is the leftmost symbol of our "macro symbol", if we start from the left (i.e. mm_dir == DIR_RIGHT).
	 */
	const int init_pos = mm_in_dir == DIR_LEFT ? scale : 1;
	const int tape_len = scale + 2;

	struct tm_run_t *slow_run = tm_run_init(tm_def, 0, tape_len, init_pos);
	slow_run->state = tm_in_state;
	// It's a bit ugly to modify our object like this, but it would be too bothersome to
	// implement a parameter for the initial tape state. Perhaps we can implement a sensible API such as having an offset in tm_flat_tape_write();
	for (int i = 0; i < scale; i++) {
		// The symbols of the TM are given by the bits of the symbols of the MM
		const sym_t tm_sym = (mm_in_sym >> i) & 1U;
		// Note the index, we store the bits so they "look" right
		// Also recall the offset so that the highest bit should be located at mem_pos == 1
		// and the lowest bit at mem_pos == tape_len - 2
		const int mem_pos = scale - i;
		assert(1 <= mem_pos && mem_pos < tape_len - 1);
		slow_run->flat_tape->syms[mem_pos] = tm_sym;
	}

	struct tm_run_t *fast_run = tm_run_init(tm_def, 0, tape_len, init_pos);
	fast_run->state = tm_in_state;
	for (int i = 0; i < scale; i++) {
		const sym_t tm_sym = (mm_in_sym >> i) & 1U;
		const int mem_pos = scale - i;
		assert(1 <= mem_pos && mem_pos < tape_len - 1);
		fast_run->flat_tape->syms[mem_pos] = tm_sym;
	}

	dir_t mm_out_dir;
	while (1) {
		if (tm_run_halted(slow_run)) {
			mm_out_dir = DIR_RIGHT; // value does not matter, because out_state will be halting
			goto halted_or_escaped;
		}

		// Check if we ran outside the mm_in_sym part of the tape
		const int rel_pos = slow_run->flat_tape->rel_pos;
		if ((mm_in_dir == DIR_LEFT && rel_pos <= -scale) || (mm_in_dir == DIR_RIGHT && rel_pos <= -1)) {
			// Ran off the tape going to the left
			mm_out_dir = DIR_LEFT;
			goto halted_or_escaped;
		}
		if ((mm_in_dir == DIR_RIGHT && rel_pos >= scale) || (mm_in_dir == DIR_LEFT && rel_pos >= 1)) {
			// Ran off the tape going to the right
			mm_out_dir = DIR_RIGHT;
			goto halted_or_escaped;
		}

		tm_run_step(slow_run);

		for (int i = 0; i < 2; i++) {
			if (tm_run_halted(fast_run)) {
				mm_out_dir = DIR_RIGHT; // value does not matter, because out_state will be halting
				goto halted_or_escaped;
			}

			// Check if we ran outside the mm_in_sym part of the tape
			const int rel_pos = fast_run->flat_tape->rel_pos;
			if ((mm_in_dir == DIR_LEFT && rel_pos <= -scale) || (mm_in_dir == DIR_RIGHT && rel_pos <= -1)) {
				// Ran off the tape going to the left
				mm_out_dir = DIR_LEFT;
				goto halted_or_escaped;
			}
			if ((mm_in_dir == DIR_RIGHT && rel_pos >= 1) || (mm_in_dir == DIR_LEFT && rel_pos >= scale)) {
				// Ran off the tape going to the right
				mm_out_dir = DIR_RIGHT;
				goto halted_or_escaped;
			}

			tm_run_step(fast_run);
		}

		int cmp = 1; // FIXME dummy value
		// int cmp = tm_flat_tape_cmp(slow_run->flat_tape, fast_run->flat_tape);
	}
halted_or_escaped:
	(void) 1; // Strange hack to handle C99 syntax quirk

	// Construct the instruction for the MM
	struct tm_instr_t mm_instr;

	// NB if we want to support MMs for TMs with more than two symbols, we should shift by (tm_def->n_syms / 2) here instead of 1
	for (int i = 0; i < scale; i++) {
		const int mem_pos = scale - i;
		const sym_t tm_sym = slow_run->flat_tape->syms[mem_pos];
		mm_instr.sym |= tm_sym << i;
	}

	mm_instr.state = (slow_run->state << 1) | mm_out_dir;
	mm_instr.dir = mm_out_dir;

	tm_run_free(slow_run);
	tm_run_free(fast_run);

	return mm_instr;
}

/*
 * Creates a Macro Machine by scaling up the given definition by a given factor and introducing
 * directed states. That means that for each "scale" symbols in the TM, the MM contains one symbol.
 * The following holds:
 * mm_def->n_syms = tm_def->n_syms << (scale - 1)	as many syms are packed into one
 * mm_def->n_states = tm_def->n_states * 2			as each state is directed
 * For now we just support tm_def->n_syms == 2, but we may add more later
 */
struct tm_def_t *tm_def_to_mm_def(const struct tm_def_t *const tm_def, const int scale) {
	if (tm_def->n_syms != 2) {
		// TODO it's "easy" to fix this, just use floor_log2() and waste a few bits in the macro symbols (perhaps, the symbol table will be sparse then...)
		ERROR("Can (currently) only produce MMs for TMs with 2 symbols, got %d.", tm_def->n_syms);
	}
	
	struct tm_def_t *mm_def = tm_def_init(tm_def->n_syms << (scale - 1), tm_def->n_states * 2);

	for (int state = 0; state < mm_def->n_states; state++) {
		for (int sym = 0; sym < mm_def->n_syms; sym++) {
			struct tm_instr_t instr = mm_determine_instr(tm_def, scale, state, sym);
			tm_def_store(mm_def, state, sym, instr);
		}
	}

	return mm_def;
}
