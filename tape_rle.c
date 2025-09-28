#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "tape.h"
#include "util.h"

#include "tape_rle.h"

/*
 * An element of a RLE linked list. If left or right are NULL
 * this represents the empty edge of the tape "0^inf".
 */
struct rle_elem_t {
	struct rle_elem_t *left;	// the left neighbor (may be NULL);
	struct rle_elem_t *right;	// the right neighbor (may be NULL)
	sym_t sym;					// the symbol
	int len;					// length of the run, always positive
};

/*
 * Constructs an "initial" RLE element, which has no left or right
 * neighbor, with the given symbol and length.
 */
static struct rle_elem_t *rle_elem_init(const sym_t sym, const int len)
{
	struct rle_elem_t *elem = malloc(sizeof *elem);
	elem->left = NULL;
	elem->right = NULL;
	elem->sym = sym;
	elem->len = len;
	return elem;
}

/*
 * Shrinks the given element by one symbol. If the length
 * is zero it is removed from the tape and its neighbors
 * are linked instead. The element's memory is then freed.
 */
static void rle_elem_shrink(struct rle_elem_t *const elem)
{
	elem->len--;
	if (elem->len <= 0) {
		if (elem->left)
			elem->left->right = elem->right;
		if (elem->right)
			elem->right->left = elem->left;
		free(elem);
	}
}

/*
 * Links two RLEs by setting the neighbor pointers as appropriate.
 * NULLs are allowed as inputs.
 */
static void rle_elem_link(struct rle_elem_t *const left, struct rle_elem_t *const right)
{
	if (left)
		left->right = right;
	if (right)
		right->left = left;
}

/*
 * A run-length encoding based tape representation as
 * a linked list of runs of repeating symbols.
 */
struct rle_tape_t {
	struct rle_elem_t *curr;	// current element
	int rle_pos; 				// position within the RLE
	int rel_pos;				// relative position (0 = starting)
	unsigned sym_bits; 			// the width of one symbol in bits (must be <= MAX_SYM_BITS)
};


/*
 * Frees a RLE tape together with all its elements.
 */
void rle_tape_free(struct tape_t *const tape)
{
	struct rle_tape_t *data = tape->data;
	struct rle_elem_t *elem = data->curr;
	// Move to the leftmost elem
	while (elem->left) elem = elem->left;
	// Free each element
	while (elem) {
		struct rle_elem_t *next = elem->right;
		free(elem);
		elem = next;
	}
	// Free data container struct
	free(data);
	// Free outer container struct
	free(tape);
}

/*
 * Constructs a new blank tape using RLE representation, for
 * a given symbol width.
 */
struct tape_t *rle_tape_init(const unsigned sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct rle_tape_t *data = malloc(sizeof *data);
	data->curr = rle_elem_init(0, 1);
	data->rle_pos = 0;
	data->rel_pos = 0;
	data->sym_bits = sym_bits;

	struct tape_t *tape = malloc(sizeof *tape);
	tape->data = data;
	tape->free = rle_tape_free;
	tape->read = rle_tape_read;
	tape->write = rle_tape_write;
	tape->move = rle_tape_move;
	return tape;
}

/*
 * Prints an entire RLE tape.
 */
void rle_tape_print(const struct rle_tape_t *const tape, const state_t state, const int directed)
{
	const struct rle_elem_t *elem = tape->curr;
	// Move to the leftmost elem
	while (elem->left) elem = elem->left;

	printf("... ");
	do {
		if (elem == tape->curr) {
			// Print tape head within element
			// NB here we use '_' instead of ' ' for separators
			const int left_len = tape->rle_pos;
			const int right_len = elem->len - tape->rle_pos - 1;

			// Before tape head
			if (left_len > 0) {
				sym_bin_print(elem->sym, tape->sym_bits);
				printf("^%d_", left_len);
			}

			// Tape head and state
			print_state_and_sym(state, elem->sym, tape->sym_bits, directed);
			printf("%c", right_len > 0 ? '_' : ' ');

			// After tape head
			if (right_len > 0) {
				sym_bin_print(elem->sym, tape->sym_bits);
				printf("^%d ", right_len);
			}
		} else {
			// Print the entire element
			sym_bin_print(elem->sym, tape->sym_bits);
			printf("^%d ", elem->len);
		}
		elem = elem->right;
	} while (elem);
	printf("...\n");
}

/*
 * Writes to the current RLE, or the left or right, as appropriate.
 * Thus may modify the current RLE and the current position.
 */
void rle_tape_write(struct tape_t *const tape, const sym_t sym)
{
	struct rle_tape_t *const data = tape->data;

	struct rle_elem_t *const orig = data->curr;
	if (orig->sym == sym) {
		// Nothing to do
		return;
	}

	if (data->rle_pos == 0 && orig->left && orig->left->sym == sym) {
		// Extend left neighbor
		data->curr = orig->left;
		data->curr->len++;
		data->rle_pos = data->curr->len - 1;

		rle_elem_shrink(orig);
		return;
	}

	if (data->rle_pos == orig->len && orig->right && orig->right->sym == sym) {
		// Extend right neighbor
		data->curr = orig->right;
		data->curr->len++;
		data->rle_pos = 0;

		rle_elem_shrink(orig);
		return;
	}

	// Create new in middle / left / right
	struct rle_elem_t *const new_mid = rle_elem_init(sym, 1);

	const int left_len = data->rle_pos;
	if (left_len > 0) {
		// We have "leftover" symbols to the left
		struct rle_elem_t *const new_left = rle_elem_init(orig->sym, left_len);
		rle_elem_link(orig->left, new_left);
		rle_elem_link(new_left, new_mid);
	} else {
		rle_elem_link(orig->left, new_mid);
	}

	const int right_len = orig->len - data->rle_pos - 1;
	if (right_len > 0) {
		// We have "leftover" symbols to the left
		struct rle_elem_t *const new_right = rle_elem_init(orig->sym, right_len);
		rle_elem_link(new_right, orig->right);
		rle_elem_link(new_mid, new_right);
	} else {
		rle_elem_link(new_mid, orig->right);
	}

	if (orig->left)
		assert(orig->left->right == new_mid || orig->left->right == new_mid->left);
	if (orig->right)
		assert(orig->right->left == new_mid || orig->right->left == new_mid->right);
	assert(new_mid->len == 1);
	assert(new_mid->sym == sym);

	// Update tape pointers and free removed element
	data->curr = new_mid;
	data->rle_pos = 0;
	free(orig);
}

/*
 * Reads a symbol from the RLE tape.
 */
sym_t rle_tape_read(const struct tape_t *const tape)
{
	const struct rle_tape_t *const data = tape->data;
	return data->curr->sym;
}

/*
 * Moves in the RLEs by modifying the relative position and, if needed,
 * the current element pointer. If we move out of the current tape, a
 * new element consisting of one zero is created, to represent one
 * piece of the infinite blank tape. Note that if the current element's
 * symbol is a zero, we instead excent the current element.
 */
void rle_tape_move(struct tape_t *const tape, int delta)
{
	struct rle_tape_t *const data = tape->data;

	assert(delta == -1 || delta == 1);
	// This defines the min/max allowed tape head positions, namely INT_MIN+1 and INT_MAX-1
	assert(delta == -1 || delta == 1);
	if (delta == -1)
		assert(INT_MIN + 2 < data->rel_pos && data->rel_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < data->rel_pos && data->rel_pos < INT_MAX - 2);

	// First update the relative position
	data->rel_pos += delta;

	struct rle_elem_t *const orig = data->curr;

	if (data->rle_pos + delta < 0) {
		// Moving out of the element, to the left side
		if (orig->left == NULL) {
			// End of tape
			if (orig->sym == 0) {
				// Extend the current run of zeros by one
				orig->len++;
				assert(data->rle_pos == 0); // by assumption
			} else {
				// Create a new zero to the left and move into it
				struct rle_elem_t *const new_left = rle_elem_init(0, 1);
				rle_elem_link(new_left, orig);
				data->curr = orig->left;
				data->rle_pos = data->curr->len - 1;
			}
		} else {
			// Move into the existing left element
			data->curr = orig->left;
			data->rle_pos = data->curr->len - 1;
		}
		return;
	}

	if (data->rle_pos + delta >= orig->len) {
		// Moving out of the element, to the right
		if (orig->right == NULL) {
			if (orig->sym == 0) {
				// Extend the current run of zeros by one
				orig->len++;
				data->rle_pos++;
				assert(data->rle_pos == orig->len - 1); // by assumption
			} else {
				// Create a new zero to the right and move into it
				struct rle_elem_t *const new_right = rle_elem_init(0, 1);
				rle_elem_link(orig, new_right);
				data->curr = orig->right;
				data->rle_pos = data->curr->len - 1;
			}
		} else {
			// Move into the existing right element
			data->curr = orig->right;
			data->rle_pos = 0;
		}
		return;
	}

	// Simply move within the element
	data->rle_pos += delta;
	assert(0 <= data->rle_pos && data->rle_pos < data->curr->len); // check invariants
}

/*
 * Counts the total number of nonzero symbols in the tape
 * by doing a full scan of the entire tape
 */
static int rle_count_nonzero(const struct rle_tape_t *const tape)
{
	const struct rle_elem_t *elem = tape->curr;
	int nonzero = 0;

	// Count current and to the left
	while (elem) {
		if (elem->sym != 0)
			nonzero += elem->len;
		elem = elem->left;
	}

	// Count (strictly) to the right of current
	assert(tape->curr != NULL); // to make linter happy
	elem = tape->curr->right; // already counted tape->curr
	while (elem) {
		if (elem->sym != 0)
			nonzero += elem->len;
		elem = elem->right;
	}

	return nonzero;
}
