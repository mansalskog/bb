#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tm.h"

/*
 * Definition of our symbol type. Using char allows us up to
 * 256 symbols, which lets us define 8-MMs at the highest.
 */
typedef unsigned char sym_t;
// NB. max sym bits should always fit in an int.
const int MAX_SYM_BITS = (int) (CHAR_BIT * sizeof (sym_t));

/*
 * Move direction representation, is actually only a single bit.
 */
enum {
	DIR_LEFT = 0,
	DIR_RIGHT = 1,
};
typedef unsigned char dir_t;

// Definition of our state type. This allows us up to 256 states.
typedef unsigned char state_t;
// Sentinel value for representing undefined states.
const state_t STATE_UNDEF = 'Z' - 'A';

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
 * A flat tape that automatically expands (by doubling the size) if we run off the edge.
 */
struct flat_tape_t {
	sym_t *syms;		// symbol memory array
	int len;			// length of the full tape
	int rel_pos;		// relative position (0 = TM starting position)
	int init_pos;		// initial position, to ensure mem_pos := rel_pos + init_pos is always within the array
	unsigned sym_bits;	// the width of one symbol in bits (must be <= MAX_SYM_BITS)
	// invariants: 0 <= rel_pos + init_pos < len
};


/*
 * Represents an "instruction" in the transition table.
 */
struct tm_instr_t {
	sym_t sym;			// the symbol to write
	dir_t dir;		// the move direction; it is only one bit
	state_t state;		// the next state to enter
};

/*
 * A definition of a Turing Machine transition table, as a static structure.
 * The function is (in_state, in_sym) -> (out_state, out_sym, move_dir) where
 * idx = in_state * n_syms + in_sym
 * out_state = state_tab[idx]
 * out_sym = sym_tab[idx]
 * move_dir = (move_dirs[idx / DIR_TAB_BITS_PER_FIELD] >> (idx % DIR_TAB_BITS_PER_FIELD)) & 1;
 * Note that we can get sym_bits from a tm_def_t by sym_bits = floor_log2(n_syms)
 */
struct tm_def_t {
	sym_t *sym_tab;				// symbol transition table
	state_t *state_tab;			// state transition table
	unsigned long *dir_tab;		// direction table as packed bit-fields
	int n_syms;					// number of symbols
	int n_states;				// number of states
};

const int DIR_TAB_BITS_PER_FIELD = CHAR_BIT * sizeof (unsigned long);

///// Basic utility and output functions /////

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')
// Error macro; NOTE must be called with constant string as first arg
#define ERROR(...) ((void) fprintf(stderr, "ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) ((void) fprintf(stderr, "WARNING: " __VA_ARGS__))

/*
 * The maximum of two integers, used for clamping values etc.
 */
int maximum(const int a, const int b)
{
	return a > b ? a : b;
}

/*
 * Calculates how many bits are needed to fit a value, e.g. floor_log2(7) = 3.
 * Mathematically, this is equivalent to floor(log2(n)) or
 * the number m such that n >> (m + 1) == 0.
 */
unsigned floor_log2(const int n)
{
	assert(n >= 0); // Only valid for non-negative integers
	unsigned shift = 0;
	while ((unsigned) n >> (shift + 1) > 0)
		shift++;
	return shift;
}

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

///// RLE functions /////

/*
 * Frees a RLE tape together with all its elements.
 */
void rle_tape_free(struct rle_tape_t *const tape)
{
	struct rle_elem_t *elem = tape->curr;
	// Move to the leftmost elem
	while (elem->left) elem = elem->left;
	// Free each element
	while (elem) {
		struct rle_elem_t *next = elem->right;
		free(elem);
		elem = next;
	}
	// Free container struct
	free(tape);
}

/*
 * Constructs an "initial" RLE element, which has no left or right
 * neighbor, with the given symbol and length.
 */
struct rle_elem_t *rle_elem_init(const sym_t sym, const int len)
{
	struct rle_elem_t *elem = malloc(sizeof *elem);
	elem->left = NULL;
	elem->right = NULL;
	elem->sym = sym;
	elem->len = len;
	return elem;
}

/*
 * Constructs a new blank tape using RLE representation, for
 * a given symbol width.
 */
struct rle_tape_t *rle_tape_init(const unsigned sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct rle_tape_t *tape = malloc(sizeof *tape);
	tape->curr = rle_elem_init(0, 1);
	tape->rle_pos = 0;
	tape->rel_pos = 0;
	tape->sym_bits = sym_bits;
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
 * Shrinks the given element by one symbol. If the length
 * is zero it is removed from the tape and its neighbors
 * are linked instead. The element's memory is then freed.
 */
void rle_elem_shrink(struct rle_elem_t *const elem)
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
void rle_elem_link(struct rle_elem_t *const left, struct rle_elem_t *const right)
{
	if (left)
		left->right = right;
	if (right)
		right->left = left;
}

/*
 * Writes to the current RLE, or the left or right, as appropriate.
 * Thus may modify the current RLE and the current position.
 */
void rle_tape_write(struct rle_tape_t *const tape, sym_t sym)
{
	struct rle_elem_t *const orig = tape->curr;
	if (orig->sym == sym) {
		// Nothing to do
		return;
	}

	if (tape->rle_pos == 0 && orig->left && orig->left->sym == sym) {
		// Extend left neighbor
		tape->curr = orig->left;
		tape->curr->len++;
		tape->rle_pos = tape->curr->len - 1;

		rle_elem_shrink(orig);
		return;
	}

	if (tape->rle_pos == orig->len && orig->right && orig->right->sym == sym) {
		// Extend right neighbor
		tape->curr = orig->right;
		tape->curr->len++;
		tape->rle_pos = 0;

		rle_elem_shrink(orig);
		return;
	}

	// Create new in middle / left / right
	struct rle_elem_t *const new_mid = rle_elem_init(sym, 1);

	const int left_len = tape->rle_pos;
	if (left_len > 0) {
		// We have "leftover" symbols to the left
		struct rle_elem_t *const new_left = rle_elem_init(orig->sym, left_len);
		rle_elem_link(orig->left, new_left);
		rle_elem_link(new_left, new_mid);
	} else {
		rle_elem_link(orig->left, new_mid);
	}

	const int right_len = orig->len - tape->rle_pos - 1;
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
	tape->curr = new_mid;
	tape->rle_pos = 0;
	free(orig);
}

/*
 * Reads a symbol from the RLE tape.
 */
sym_t rle_tape_read(const struct rle_tape_t *const tape)
{
	return tape->curr->sym;
}

/*
 * Moves in the RLEs by modifying the relative position and, if needed,
 * the current element pointer. If we move out of the current tape, a
 * new element consisting of one zero is created, to represent one
 * piece of the infinite blank tape. Note that if the current element's
 * symbol is a zero, we instead excent the current element.
 */
void rle_tape_move(struct rle_tape_t *const tape, int delta)
{
	assert(delta == -1 || delta == 1);
	// This defines the min/max allowed tape head positions, namely INT_MIN+1 and INT_MAX-1
	assert(delta == -1 || delta == 1);
	if (delta == -1)
		assert(INT_MIN + 2 < tape->rel_pos && tape->rel_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < tape->rel_pos && tape->rel_pos < INT_MAX - 2);

	// First update the relative position
	tape->rel_pos += delta;

	struct rle_elem_t *const orig = tape->curr;

	if (tape->rle_pos + delta < 0) {
		// Moving out of the element, to the left side
		if (orig->left == NULL) {
			// End of tape
			if (orig->sym == 0) {
				// Extend the current run of zeros by one
				orig->len++;
				assert(tape->rle_pos == 0); // by assumption
			} else {
				// Create a new zero to the left and move into it
				struct rle_elem_t *const new_left = rle_elem_init(0, 1);
				rle_elem_link(new_left, orig);
				tape->curr = orig->left;
				tape->rle_pos = tape->curr->len - 1;
			}
		} else {
			// Move into the existing left element
			tape->curr = orig->left;
			tape->rle_pos = tape->curr->len - 1;
		}
		return;
	}

	if (tape->rle_pos + delta >= orig->len) {
		// Moving out of the element, to the right
		if (orig->right == NULL) {
			if (orig->sym == 0) {
				// Extend the current run of zeros by one
				orig->len++;
				tape->rle_pos++;
				assert(tape->rle_pos == orig->len - 1); // by assumption
			} else {
				// Create a new zero to the right and move into it
				struct rle_elem_t *const new_right = rle_elem_init(0, 1);
				rle_elem_link(orig, new_right);
				tape->curr = orig->right;
				tape->rle_pos = tape->curr->len - 1;
			}
		} else {
			// Move into the existing right element
			tape->curr = orig->right;
			tape->rle_pos = 0;
		}
		return;
	}

	// Simply move within the element
	tape->rle_pos += delta;
	assert(0 <= tape->rle_pos && tape->rle_pos < tape->curr->len); // check invariants
}

/*
 * Counts the total number of nonzero symbols in the tape
 * by doing a full scan of the entire tape
 */
int rle_count_nonzero(const struct rle_tape_t *const tape)
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

///// Flat tape functions /////

/*
 * Frees the memory used by a flat tape.
 */
void flat_tape_free(struct flat_tape_t *const tape)
{
	free(tape->syms);
	free(tape);
}

/*
 * Creates a flat tape of the specified initial size filled with zeros.
 * The absolute position is initialized to zero.
 */
struct flat_tape_t *flat_tape_init(const int len, const int init_pos, const unsigned sym_bits)
{
	assert(0 <= init_pos && init_pos < len);			// This ensures that our intial position is within the syms array
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct flat_tape_t *const tape = malloc(sizeof *tape);
	tape->syms = malloc(len * sizeof *tape->syms);
	tape->len = len;
	tape->rel_pos = 0;
	tape->init_pos = init_pos;
	tape->sym_bits = sym_bits;

	return tape;
}

/*
 * Prints an excerpt of the flat tape with a given ctx number of symbols
 * on either side of the head.
 */
void flat_tape_print(const struct flat_tape_t *const tape, const int ctx, const state_t state, const int directed)
{
	const int mem_pos = tape->rel_pos + tape->init_pos;
	// The left limit, may be less than 0
	const int left = mem_pos - ctx;
	// Number of symbols outside the tape to print (distance from 0th sym)
	const int left_out = maximum(0 - left, 0);
	assert(left + left_out >= 0);

	// The right limit, may be larger than len - 1
	const int right = mem_pos + ctx + 1;
	// Number of symbols outside the tape to print (distance from last sym)
	const int right_out = maximum(right - (tape->len - 1), 0);
	assert(right - right_out < tape->len);

	// Tape before head
	for (int i = 0; i < left_out; i++)
		printf(".");
	for (int i = left + left_out; i < mem_pos; i++) {
		sym_bin_print(tape->syms[i], tape->sym_bits);
		printf(" ");
	}

	// Head and current state
	print_state_and_sym(state, tape->syms[mem_pos], tape->sym_bits, directed);
	printf(" ");

	// Tape after head
	for (int i = mem_pos + 1; i <= right - right_out; i++) {
		sym_bin_print(tape->syms[i], tape->sym_bits);
		printf(" ");
	}

	for (int i = 0; i < right_out; i++)
		printf(".");
	printf("\n");
}

/*
 * Moves left or right in the flat tape, reallocating tape as required if we move outside.
 * NOTE: it is not very efficient to allocate 100% more memory just because we went outside
 * on one side by one symbol. Perhaps we should be later change to only add 50% more on the
 * side we actually went outside. Depends on the statistical properties of TMs, e.g.
 * "bouncers" (allocate both sides is good) vs "translated cycles" (allocate one side is good), etc.
 */
void flat_tape_move(struct flat_tape_t *const tape, int delta)
{
	// This defines the min/max allowed tape head positions, namely INT_MIN+1 and INT_MAX-1
	assert(delta == -1 || delta == 1);
	if (delta == -1)
		assert(INT_MIN + 2 < tape->rel_pos && tape->rel_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < tape->rel_pos && tape->rel_pos < INT_MAX - 2);

	const int mem_pos = tape->rel_pos + tape->init_pos;
	if (mem_pos + delta < 0 || mem_pos + delta >= tape->len) {
		assert(0); // FIXME
		// Ran out of tape, allocate more
		const int old_len = tape->len;
		const int new_len = old_len * 2;
		sym_t *const new_syms = malloc(sizeof *new_syms * new_len);

		/* Place data in the "middle" of the tape, growing it by about 50% on both sides
		 * We need to increment the offset by half of the old length, which may be one unit
		 * "too little" if the old_len was not divisible by two, but as long as we copy
		 * everything correctly, this does not matter, as next time the tape will move by
		 * exactly half of new_len.
		 */
		const int offset = old_len / 2;
		memset(new_syms, 0, offset * sizeof *new_syms);
		memcpy(new_syms + (ptrdiff_t) offset, tape->syms, old_len * sizeof *new_syms);
		memset(new_syms + (ptrdiff_t) (offset + old_len), 0, (new_len - old_len - offset) * sizeof *new_syms);

		tape->len = new_len;
		free(tape->syms);
		tape->syms = new_syms;
		tape->init_pos += offset;
	}

	tape->rel_pos += delta;
	assert(0 <= tape->rel_pos + tape->init_pos && tape->rel_pos + tape->init_pos < tape->len);
}

/*
 * Writes a symbol to the tape.
 */
void flat_tape_write(const struct flat_tape_t *const tape, const sym_t sym)
{
	tape->syms[tape->rel_pos + tape->init_pos] = sym;
}

/*
 * Reads a symbol from the tape.
 */
sym_t flat_tape_read(const struct flat_tape_t *const tape)
{
	return tape->syms[tape->rel_pos + tape->init_pos];
}

/*
 * Counts the number of nonzero symbols in a flat tape, for use in e.g. the BB sigma function.
 * This requires a full scan of the tape. We could perhapst add variables for the leftmost and
 * rightmost position visited, but this is probably really fast.
 */
int flat_tape_count_nonzero(const struct flat_tape_t *const tape) {
	int nonzero = 0;
	for (int i = 0; i < tape->len; i++) {
		if (tape->syms[i] != 0)
			nonzero++;
	}
	return nonzero;
}

///// TM definition (transition table) functions /////

/*
 * Stores a given instruction in the transition table.
 */
void tm_def_store(const struct tm_def_t *const def, const state_t state, const sym_t sym, const struct tm_instr_t instr)
{
	assert(0 <= state && state < def->n_states);
	assert(0 <= sym && sym < def->n_syms);

	// assert(0 <= instr.state && instr.state < def->n_states); // TODO allow for 0xFF state etc.
	assert(0 <= instr.sym && instr.sym < def->n_syms);
	assert(instr.dir == DIR_LEFT || instr.dir == DIR_RIGHT);

	const int idx = state * def->n_syms + sym;
	def->sym_tab[idx] = instr.sym;
	def->state_tab[idx] = instr.state;

	const unsigned field_idx = idx / DIR_TAB_BITS_PER_FIELD;
	const unsigned bit_idx = idx % DIR_TAB_BITS_PER_FIELD;
	// NB we know that DIR_LEFT and DIR_RIGHT must be 0 and 1, but here we don't need to rely on which is which
	// Also note that here we assume that the type is unsigned long, so be careful with changing that!
	if (instr.dir == 0) {
		// Clear the bit
		def->dir_tab[field_idx] &= ~(1UL << bit_idx);
	} else {
		// Set the bit
		def->dir_tab[field_idx] |= 1UL << bit_idx;
	}
}

/*
 * Retrieves an instruction from the transition table.
 */
struct tm_instr_t tm_def_lookup(const struct tm_def_t *const def, const state_t state, const sym_t sym)
{
	assert(0 <= state && state < def->n_states);
	assert(0 <= sym && sym < def->n_syms);

	const int idx = state * def->n_syms + sym;
	struct tm_instr_t instr;
	instr.sym = def->sym_tab[idx];
	instr.state = def->state_tab[idx];

	const unsigned field_idx = idx / DIR_TAB_BITS_PER_FIELD;
	const unsigned bit_idx = idx % DIR_TAB_BITS_PER_FIELD;
	instr.dir = (def->dir_tab[field_idx] >> bit_idx) & 1UL;

	return instr;
}

/*
 * Allocates an empty transition table of the specified size. We must fill it with states
 * before running.
 */
struct tm_def_t *tm_def_init(const int n_syms, const int n_states)
{
	assert(n_syms > 0 && n_states > 0);
	struct tm_def_t *def = malloc(sizeof *def);
	def->n_syms = n_syms;
	def->n_states = n_states;
	const int tab_size = n_syms * n_states;
	def->sym_tab = malloc(tab_size * sizeof *def->sym_tab);
	def->state_tab = malloc(tab_size * sizeof *def->state_tab);
	def->dir_tab = malloc(tab_size * sizeof *def->dir_tab / DIR_TAB_BITS_PER_FIELD);
	return def;
}

/*
 * Frees a TM definition (transition table).
 */
void tm_def_free(struct tm_def_t *const def)
{
	free(def->sym_tab);
	free(def->state_tab);
	free(def->dir_tab);
	free(def);
}


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
	const int n_states = (int) ((len + 1) / (n_syms * 3 + 1));

	struct tm_def_t *def = tm_def_init(n_syms, n_states);

	for (int i_state = 0; i_state < def->n_states; i_state++) {
		for (int i_sym = 0; i_sym < def->n_syms; i_sym++) {
			const int txt_idx = i_state * (def->n_syms * 3 + 1) + i_sym * 3; // base index into txt

			const char sym_c = txt[txt_idx];
			if (!sym_c || sym_c < '0' || sym_c - '0' >= def->n_syms) {
				// Allow unused states only if all three chars are '-'
				// NB. short-circuiting prevents reading past end of string here
				if (sym_c == '-' && txt[txt_idx + 1] == '-' && txt[txt_idx + 2] == '-') {
					const struct tm_instr_t instr = {
						0, // dummy, will not be read
						DIR_LEFT, // dummy, will not be read
						STATE_UNDEF,
					};
					tm_def_store(def, i_state, i_sym, instr);
					continue;
				}
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym_c), i_state, i_sym, '0' + def->n_syms - 1);
			}

			char dir_c = txt[txt_idx + 1];
			if (!dir_c || (dir_c != 'L' && dir_c != 'R')) {
				ERROR("Invalid direction %c at row %d col %d.\n",
					ONLY_ALNUM(dir_c), i_state, i_sym);
			}

			char state_c = txt[txt_idx + 2];
			if (!state_c || state_c < 'A' || state_c - 'A' >= def->n_states) {
				if (state_c != 'Z' && state_c != 'H') {
					WARN("Unusual halting state state %c at row %d col %d, should be either A-%c or H or Z.\n",
						ONLY_ALNUM(state_c), i_state, i_sym, 'A' + def->n_states - 1);
				}
			}
			// Save the instruction
			const struct tm_instr_t instr = {
				sym_c - '0',
				dir_c == 'L' ? DIR_LEFT : DIR_RIGHT,
				state_c - 'A',
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
	for (int i_state = 0; i_state < def->n_states; i_state++) {
		printf("\n%c ", 'A' + i_state);
		for (int i_sym = 0; i_sym < def->n_syms; i_sym++) {
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

///// TM run functions /////

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

///// Verification utility functions /////

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

///// Actual TM analysis code /////

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
