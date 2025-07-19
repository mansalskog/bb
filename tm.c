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
 * Hopefully the compiler is smart enought to store this as a char (maybe assert that)?
 */
enum dir_t {
	DIR_LEFT = 0,
	DIR_RIGHT = 1,
};

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
	struct rle_elem_t *curr; // current element
	int rle_pos; 			// relative position within the RLE
	int abs_pos;			// absolute position (0 = starting)
	int sym_bits; 		// the width of one symbol in bits (must be <= MAX_SYM_BITS)
};

/*
 * A flat tape that automatically expands (by doubling the size) if we run off the edge.
 */
struct flat_tape_t {
	sym_t *syms;	// symbol memory array
	int len;		// length of the full tape
	int mem_pos;	// memory position (0 = first symbol of array)
	int abs_pos;	// absolute position (0 = TM starting position)
	int sym_bits;	// the width of one symbol in bits (must be <= MAX_SYM_BITS)
	// invariants: len % 2 == 0, rel_pos == abs_pos + len / 2
};


/*
 * Represents an "instruction" in the transition table.
 */
struct tm_instr_t {
	sym_t sym;		// the symbol to write
	enum dir_t dir;		// the move direction; it is only one bit
	state_t state;	// the next state to enter
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
int floor_log2(const int n)
{
	assert(n >= 0); // Only valid for non-negative integers
	int shift = 0;
	while (n >> (shift + 1) > 0) shift++;
	return shift;
}

/*
 * Prints a state as a letter from A to Z. This means we only
 * support up to 26 states. We probably don't need more, but
 * if we do we have to invent a format for them.
 */
char state_to_char(const state_t state)
{
	if (state > 'Z' - 'A') {
		ERROR(
			"Can currently only print states A-Z (0-%d), tried to print %d.",
			'Z' - 'A',
			state);
	}
	return (char) ('A' + state);
}

/*
 * Prints a symbol as a string of binary digits to stdout.
 * The width is fixed (leading zeros included).
 */
void sym_bin_print(const sym_t sym, const int sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);
	for (int i = sym_bits - 1; i >= 0; i--) {
		// Don't bother shifting down because we only care if zero or not
		const sym_t bit = sym & (1 << i);
		printf("%c", bit ? '1' : '0');
	}
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
struct rle_tape_t *rle_tape_init(const int sym_bits)
{
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct rle_tape_t *tape = malloc(sizeof *tape);
	tape->curr = rle_elem_init(0, 1);
	tape->rle_pos = 0;
	tape->abs_pos = 0;
	tape->sym_bits = sym_bits;
	return tape;
}

/*
 * Prints an entire RLE tape.
 */
void rle_tape_print(const struct rle_tape_t *const tape, const state_t state, const int prev_delta)
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
			if (prev_delta == -1) {
				sym_bin_print(elem->sym, tape->sym_bits);
				printf("<%c", state_to_char(state));
			} else if (prev_delta == 1) {
				printf("%c>", state_to_char(state));
				sym_bin_print(elem->sym, tape->sym_bits);
			} else {
				// special case for non-started TMs (before first step)
				assert(prev_delta == 0);

				printf("[");
				sym_bin_print(elem->sym, tape->sym_bits);
				printf("]%c", state_to_char(state));
			}
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
		if (elem->left) elem->left->right = elem->right;
		if (elem->right) elem->right->left = elem->left;
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

	if (orig->left) assert(orig->left->right == new_mid || orig->left->right == new_mid->left);
	if (orig->right) assert(orig->right->left == new_mid || orig->right->left == new_mid->right);
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
		assert(INT_MIN + 2 < tape->abs_pos && tape->abs_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < tape->abs_pos && tape->abs_pos < INT_MAX - 2);

	// First update the absolute position
	tape->abs_pos += delta;

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
		if (elem->sym != 0) nonzero += elem->len;
		elem = elem->left;
	}

	// Count (strictly) to the right of current
	assert(tape->curr != NULL); // to make linter happy
	elem = tape->curr->right; // already counted tape->curr
	while (elem) {
		if (elem->sym != 0) nonzero += elem->len;
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
struct flat_tape_t *flat_tape_init(const int len, const int sym_bits)
{
	assert(len % 2 == 0); // invariant: must be an even number
	assert(1 <= sym_bits && sym_bits <= MAX_SYM_BITS);

	struct flat_tape_t *const tape = malloc(sizeof *tape);
	tape->syms = malloc(len * sizeof *tape->syms);
	tape->len = len;
	tape->mem_pos = len / 2;
	tape->abs_pos = 0;
	tape->sym_bits = sym_bits;

	return tape;
}

/*
 * Prints an excerpt of the flat tape with a given ctx number of symbols
 * on either side of the head.
 */
void flat_tape_print(const struct flat_tape_t *const tape, const int ctx, const state_t state, const int prev_delta)
{
	// The left limit, may be negative
	const int left = tape->mem_pos - ctx;
	// Number of symbols outside the tape to print (distance from 0th sym)
	const int left_out = maximum(0 - left, 0);
	assert(left + left_out >= 0);

	// The left limit, may be larger than len - 1
	const int right = tape->mem_pos + ctx + 1;
	// Number of symbols outside the tape to print (distance from last sym)
	const int right_out = maximum(right - (tape->len - 1), 0);
	assert(right - right_out < tape->len);

	// Tape before head
	for (int i = 0; i < left_out; i++)
		printf(".");
	for (int i = left + left_out; i < tape->mem_pos; i++) {
		sym_bin_print(tape->syms[i], tape->sym_bits);
		printf(" ");
	}

	// Head and current state
	if (prev_delta == -1) {
		sym_bin_print(tape->syms[tape->mem_pos], tape->sym_bits);
		printf("<%c", state_to_char(state));
	} else if (prev_delta == 1) {
		printf("%c>", state_to_char(state));
		sym_bin_print(tape->syms[tape->mem_pos], tape->sym_bits);
	} else {
		// special case for non-started TMs (before first step)
		assert(prev_delta == 0);

		printf("[");
		sym_bin_print(tape->syms[tape->mem_pos], tape->sym_bits);
		printf("]%c", state_to_char(state));
	}
	printf(" ");

	// Tape after head
	for (int i = tape->mem_pos + 1; i < right - right_out; i++) {
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
		assert(INT_MIN + 2 < tape->abs_pos && tape->abs_pos < INT_MAX - 1);
	else
		assert(INT_MIN + 1 < tape->abs_pos && tape->abs_pos < INT_MAX - 2);

	if (tape->mem_pos + delta < 0 || tape->mem_pos + delta >= tape->len) {
		// Ran out of tape, allocate more
		const int old_len = tape->len;
		// invariant: tape len is always a mult (and usually a power) of 2
		assert(old_len % 2 == 0);
		sym_t *const new_syms = malloc(sizeof *new_syms * old_len * 2);

		// Place data in the "middle" of the tape, growing it by 50% on both sides
		const int new_qtr = old_len / 2; // the new tape's length times 1/4
		memset(new_syms, 0, new_qtr);
		memcpy(new_syms + (ptrdiff_t) new_qtr, tape->syms, (size_t) (new_qtr * 2));
		memset(new_syms + (ptrdiff_t) (new_qtr * 3), 0, new_qtr);

		tape->len = new_qtr * 4;
		free(tape->syms);
		tape->syms = new_syms;
		tape->mem_pos += new_qtr;
	}

	tape->mem_pos += delta;
	tape->abs_pos += delta;
	// invariant: abs_pos is "zeroed" at (the sym right of) the middle of the tape
	assert(tape->mem_pos == tape->abs_pos + tape->len / 2);
}

/*
 * Writes a symbol to the tape.
 */
void flat_tape_write(const struct flat_tape_t *const tape, const sym_t sym)
{
	tape->syms[tape->mem_pos] = sym;
}

/*
 * Reads a symbol from the tape.
 */
sym_t flat_tape_read(const struct flat_tape_t *const tape)
{
	return tape->syms[tape->mem_pos];
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

	const int field_idx = idx / DIR_TAB_BITS_PER_FIELD;
	const int bit_idx = idx % DIR_TAB_BITS_PER_FIELD;
	// NB we know that DIR_LEFT and DIR_RIGHT must be 0 and 1, but here we don't need to rely on which is which
	if (instr.dir == 0) {
		// Clear the bit
		def->dir_tab[field_idx] &= ~(1 << bit_idx);
	} else {
		// Set the bit
		def->dir_tab[field_idx] |= 1 << bit_idx;
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

	const int field_idx = idx / DIR_TAB_BITS_PER_FIELD;
	const int bit_idx = idx % DIR_TAB_BITS_PER_FIELD;
	instr.dir = (def->dir_tab[field_idx] >> bit_idx) & 1;

	return instr;
}

/* Parses a Turing Machine from a given standard text format
 * e.g. "1RB1LB_1LA1LZ". This is a bit ugly to deal with
 * possible input format errors...
 */
struct tm_def_t *tm_def_parse(const char *const txt)
{
	struct tm_def_t *def = malloc(sizeof *def);

	// Find first underscore and thus number of columns
	int cols = 0;
	while(txt[cols] != 0 && txt[cols] != '_') cols++;

	if (cols % 3 != 0) {
		ERROR("Invalid width %d of row, should be divisible by 3.\n", cols);
	}
	def->n_syms = cols / 3;

	const size_t len = strlen(txt);
	// Each row contains three characters per sym, and one underscode for each except the last row
	// We check that the string is well-formed below, so no length check here
	// Also this cast is OK because the number of states should fit in an int
	def->n_states = (int) ((len + 1) / (def->n_syms * 3 + 1));

	// For now we store the table row-wise in a simple way
	// but later on we might be able to optimize by changing the format...
	const int tab_size = def->n_syms * def->n_states;
	if (tab_size > 0) {
		def->sym_tab = malloc(tab_size * sizeof *def->sym_tab);
		def->state_tab = malloc(tab_size * sizeof *def->state_tab);
		def->dir_tab = malloc(tab_size * sizeof *def->dir_tab / DIR_TAB_BITS_PER_FIELD);
	} else {
		def->sym_tab = NULL;
		def->state_tab = NULL;
		def->dir_tab = NULL;
		return def;
	}

	for (int i = 0; i < def->n_states; i++) {
		for (int j = 0; j < def->n_syms; j++) {
			const int txt_idx = i * (def->n_syms * 3 + 1) + j * 3; // base index into txt

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
					tm_def_store(def, i, j, instr);
					continue;
				}
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym_c), i, j, '0' + def->n_syms - 1);
			}

			char dir_c = txt[txt_idx + 1];
			if (!dir_c || (dir_c != 'L' && dir_c != 'R')) {
				ERROR("Invalid direction %c at row %d col %d.\n",
					ONLY_ALNUM(dir_c), i, j);
			}

			char state_c = txt[txt_idx + 2];
			if (!state_c || state_c < 'A' || state_c - 'A' >= def->n_states) {
				if (state_c != 'Z' && state_c != 'H') {
					WARN("Unusual halting state state %c at row %d col %d, should be either A-%c or H or Z.\n",
						ONLY_ALNUM(state_c), i, j, 'A' + def->n_states - 1);
				}
			}
			// Save the instruction
			const struct tm_instr_t instr = {
				sym_c - '0',
				dir_c == 'L' ? DIR_LEFT : DIR_RIGHT,
				state_c - 'A',
			};
			tm_def_store(def, i, j, instr);
		}

		// Read an underscore as terminator (just to verify input format)
		const char term = txt[i * (def->n_syms * 3 + 1) + def->n_syms * 3];
		// NB after the last row we expect a null char, otherwise an '_'
		if (i < def->n_states - 1 && term != '_') {
			printf("%d\n", term);
			ERROR("Invalid row terminator %c at row %d, should be underscore.\n",
				ONLY_ALNUM(term), i);
		}
		if (i == def->n_states - 1 && term != 0) {
			ERROR("Trailing character %c at row %d, expected end of input.\n",
				ONLY_ALNUM(term), i);
		}
	}

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

/*
 * Prints the "program" of the given TM as a table.
 */
void tm_def_print(const struct tm_def_t *const def)
{
	printf(" ");
	for (int j = 0; j < def->n_syms; j++) {
		printf(" %d  ", j + 1);
	}
	// const int sym_bits = ceil_log2(def->n_syms);
	for (int i = 0; i < def->n_states; i++) {
		printf("\n%c ", 'A' + i);
		for (int j = 0; j < def->n_syms; j++) {
			struct tm_instr_t instr = tm_def_lookup(def, i, j);
			printf("%d%c%c ", // NB we use decimal here but binary in tape...
				instr.sym,
				instr.dir == DIR_LEFT ? 'L' : 'R',
				state_to_char(instr.state));
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
	rle_tape_free(run->rle_tape);
	flat_tape_free(run->flat_tape);
	free(run);
}

/*
 * Initializes a new run (tape(s)) etc. for a given TM program.
 */
struct tm_run_t *tm_run_init(const struct tm_def_t *const def)
{
	struct tm_run_t *run = malloc(sizeof *run);
	run->def = def;

	const int sym_bits = floor_log2(def->n_syms);
	run->rle_tape = rle_tape_init(sym_bits);
	// NB we use initial length 2, but we could start with a larger len as a (small) optimization
	run->flat_tape = flat_tape_init(2, sym_bits);

	run->steps = 0;
	run->state = 0;	// always start in state 0, or 'A'
	run->prev_delta = 0; // represents undefined direction
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
	const sym_t in_sym_flat = flat_tape_read(run->flat_tape);
	const sym_t in_sym_rle = rle_tape_read(run->rle_tape);
	assert(in_sym_flat == in_sym_rle);
	const sym_t in_sym = in_sym_rle;

	// Lookup the instruction
	struct tm_instr_t instr = tm_def_lookup(run->def, run->state, in_sym);
	int delta = instr.dir == DIR_LEFT ? -1 : 1;

	// Tape write and move
	flat_tape_write(run->flat_tape, instr.sym);
	flat_tape_move(run->flat_tape, delta);

	// RLE write and move
	rle_tape_write(run->rle_tape, instr.sym);
	rle_tape_move(run->rle_tape, delta);

	// Update step, state and previous delta
	run->state = instr.state;
	run->steps++;
	run->prev_delta = delta;

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
		if (halt) return 1;
	}
	return 0;
}

/*
 * Prints the tape(s) of the TM run.
 */
void tm_run_print_tape(const struct tm_run_t *const run)
{
	rle_tape_print(
			run->rle_tape,
			run->state,
			run->prev_delta);
	const int context = 5;
	flat_tape_print(
			run->flat_tape,
			context,
			run->state,
			run->prev_delta);
}

///// Verification utility functions /////

/*
 * This works the same as tm_mixed_tapes_cmp, but for two flat tapes.
 */
int tm_flat_tape_cmp(const struct flat_tape_t *tape_a, const struct flat_tape_t *tape_b)
{
	if (tape_a->abs_pos != tape_b->abs_pos)
		return TAPES_DIFF_HEAD;

	// We want the smaller tape in tape_a, so we swap if neccessary
	if (tape_a->len > tape_b->len) {
		const struct flat_tape_t *tmp = tape_a;
		tape_a = tape_b;
		tape_b = tmp;
	}

	// NB the following code has a high risk of off-by-one errors!

	// First compare the common area to the left of abs_pos = 0 (including the zero pos)
	for (int abs_pos = 0; abs_pos >= -(tape_a->len / 2); abs_pos--) {
		const int mem_pos_a = abs_pos + tape_a->len / 2;
		const int mem_pos_b = abs_pos + tape_b->len / 2;
		if (tape_a->syms[mem_pos_a] != tape_b->syms[mem_pos_b]) {
			return abs_pos;
		}
	}

	// Then compare the "extra" area in the bigger tape, to the left of 0, against implied zero symbols of the smaller tape
	for (int abs_pos = -(tape_a->len / 2) - 1; abs_pos >= -(tape_b->len / 2); abs_pos--) {
		const int mem_pos_b = abs_pos + tape_b->len / 2;
		if (tape_b->syms[mem_pos_b] != 0) {
			return abs_pos;
		}
	}

	// Then compare the common area to the right, starting with abs_pos = 1
	// Using unneccsary parenthesis to emphazhise negation.
	for (int abs_pos = 1; abs_pos < -(tape_a->len / 2); abs_pos++) {
		const int mem_pos_a = abs_pos + tape_a->len / 2;
		const int mem_pos_b = abs_pos + tape_b->len / 2;
		if (tape_a->syms[mem_pos_a] != tape_b->syms[mem_pos_b]) {
			return abs_pos;
		}
	}

	// Then compare the "extra" area to the right, against zeros
	for (int abs_pos = tape_a->len / 2 + 1; abs_pos < tape_b->len / 2; abs_pos--) {
		const int mem_pos_b = abs_pos + tape_b->len / 2;
		if (tape_b->syms[mem_pos_b] != 0) {
			return abs_pos;
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
	if (rle_tape->abs_pos != flat_tape->abs_pos)
		return TAPES_DIFF_HEAD;

	const struct rle_elem_t *elem = rle_tape->curr;

	// Compare current (head) element going left
	int mem_pos = flat_tape->mem_pos;
	for (int i = rle_tape->rle_pos; i >= 0; i--, mem_pos--) {
		if (flat_tape->syms[mem_pos] != elem->sym) {
			goto fail;
		}
	}

	// Compare all elements to the left
	elem = elem->left;
	while (elem) {
		for (int i = elem->len - 1; i >= 0; i--, mem_pos--) {
			if (flat_tape->syms[mem_pos] != elem->sym) {
				goto fail;
			}
		}
		elem = elem->left;
	}

	// Compare current (head) element going right
	elem = rle_tape->curr;
	mem_pos = flat_tape->mem_pos + 1;
	for (int i = rle_tape->rle_pos + 1; i < elem->len; i++, mem_pos++) {
		if (flat_tape->syms[mem_pos] != elem->sym) {
			goto fail;
		}
	}

	// Compare to the right
	elem = elem->right;
	while (elem) {
		for (int i = 0; i < elem->len; i++, mem_pos++) {
			if (flat_tape->syms[mem_pos] != elem->sym) {
				goto fail;
			}
		}
		elem = elem->right;
	}

	return TAPES_EQUAL;
fail:
	// NB. this computes abs_pos for a given mem_pos. See the flat_tape_move function for details.
	return mem_pos - flat_tape->len / 2;
}
