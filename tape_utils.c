/*
 * Because we want to return the position where the tapes differ (i.e. the first one found, there
 * may be more differences), we need to use these sentinel values to represent equality or
 * difference in the actual position (that is, not difference AT a certain position).
 * Note that the minimum and maximum allowed positions are INT_MIN + 1 and INT_MAX - 1, respectively.
 */
#define TAPES_EQUAL INT_MIN
#define TAPES_DIFF_HEAD INT_MAX

/*
 * This works the same as tm_mixed_tapes_cmp, but for two flat tapes.
 */
int tm_flat_tape_cmp(const struct flat_tape_t *tape_a, const struct flat_tape_t *tape_b)
{
	ERROR("Not implemented!\n"); // TODO this needs to be adapted to init_pos instead of hardcoded len / 2

	if (tape_a->rel_pos != tape_b->rel_pos)
		return TAPES_DIFF_HEAD;

	// We want the smaller tape in tape_a, so we swap if neccessary
	if (tape_a->len > tape_b->len) {
		const struct flat_tape_t *tmp = tape_a;
		tape_a = tape_b;
		tape_b = tmp;
	}

	// NB the following code has a high risk of off-by-one errors!

	// First compare the common area to the left of rel_pos = 0 (including the zero pos)
	for (int rel_pos = 0; rel_pos >= -(tape_a->len / 2); rel_pos--) {
		const int mem_pos_a = rel_pos + tape_a->init_pos;
		const int mem_pos_b = rel_pos + tape_b->init_pos;
		if (tape_a->syms[mem_pos_a] != tape_b->syms[mem_pos_b]) {
			return rel_pos;
		}
	}

	// Then compare the "extra" area in the bigger tape, to the left of 0, against implied zero symbols of the smaller tape
	for (int rel_pos = -(tape_a->len / 2) - 1; rel_pos >= -(tape_b->len / 2); rel_pos--) {
		const int mem_pos_b = rel_pos + tape_b->init_pos;
		if (tape_b->syms[mem_pos_b] != 0) {
			return rel_pos;
		}
	}

	// Then compare the common area to the right, starting with rel_pos = 1
	// Using unneccsary parenthesis to emphazhise negation.
	for (int rel_pos = 1; rel_pos < -(tape_a->len / 2); rel_pos++) {
		const int mem_pos_a = rel_pos + tape_a->init_pos;
		const int mem_pos_b = rel_pos + tape_b->init_pos;
		if (tape_a->syms[mem_pos_a] != tape_b->syms[mem_pos_b]) {
			return rel_pos;
		}
	}

	// Then compare the "extra" area to the right, against zeros
	for (int rel_pos = tape_a->len / 2 + 1; rel_pos < tape_b->len / 2; rel_pos--) {
		const int mem_pos_b = rel_pos + tape_b->init_pos;
		if (tape_b->syms[mem_pos_b] != 0) {
			return rel_pos;
		}
	}

	// All areas checked
	return TAPES_EQUAL;
}

/*
 * This works the same as tm_mixed_tapes_cmp, but for two RLE tapes.
 */
int tm_rle_tape_cmp(const struct rle_tape_t *tape_a, const struct rle_tape_t *tape_b)
{
	const struct rle_elem_t *elem_a = tape_a->curr;
	const struct rle_elem_t *elem_b = tape_b->curr;
	while (elem_a && elem_b) {
		// TODO
		break;
	}

	return TAPES_EQUAL;
}

/*
 * Compares a RLE tape representation with a flat tape representation.
 * We start from the current head position and then compare "outwards" towards either edge,
 * and then simply finish with a memcmp to verify the flat tape is empty in the same
 * areas as the RLE tape.
 * If the tapes are equal, returns the TAPES_EQUAL constaint.
 * If the tapes differ in the position of the read/write head, reutrns TAPES_DIFF_HEAD.
 * Otherwise returns the first position where we found a difference. Our algorithm guarantees
 * that this is the smallest (in absolute value) negative index, if there is one, or if not,
 * then the smallest positive index. If remains to be seen if this is a useful definition.
 */
int tm_mixed_tape_cmp(const struct rle_tape_t *rle_tape, const struct flat_tape_t *flat_tape)
{
	// First compare head position
	if (rle_tape->rel_pos != flat_tape->rel_pos)
		return TAPES_DIFF_HEAD;

	const struct rle_elem_t *elem = rle_tape->curr;

	// Compare current (head) element going left
	int mem_pos = flat_tape->rel_pos + flat_tape->init_pos;
	for (int i = rle_tape->rle_pos; i >= 0; i--, mem_pos--) {
		assert(mem_pos >= 0); // TODO this should be handled smarter than with an assert
		if (flat_tape->syms[mem_pos] != elem->sym) {
			goto fail;
		}
	}

	// Compare all elements to the left
	elem = elem->left;
	while (elem) {
		for (int i = elem->len - 1; i >= 0; i--, mem_pos--) {
			assert(mem_pos >= 0); // TODO this should be handled smarter than with an assert
			if (flat_tape->syms[mem_pos] != elem->sym) {
				goto fail;
			}
		}
		elem = elem->left;
	}

	// Compare current (head) element going right
	elem = rle_tape->curr;
	mem_pos = flat_tape->rel_pos + flat_tape->init_pos + 1;
	for (int i = rle_tape->rle_pos + 1; i < elem->len; i++, mem_pos++) {
		assert(mem_pos < flat_tape->len); // TODO this should be handled smarter than with an assert
		if (flat_tape->syms[mem_pos] != elem->sym) {
			goto fail;
		}
	}

	// Compare to the right
	elem = elem->right;
	while (elem) {
		for (int i = 0; i < elem->len; i++, mem_pos++) {
			assert(mem_pos < flat_tape->len); // TODO this should be handled smarter than with an assert
			if (flat_tape->syms[mem_pos] != elem->sym) {
				goto fail;
			}
		}
		elem = elem->right;
	}

	return TAPES_EQUAL;
fail:
	// NB. this computes rel_pos for a given mem_pos. See the flat_tape_move function for details.
	return mem_pos - flat_tape->init_pos;
}
