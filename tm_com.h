// Common TM definitions
#ifndef TM_COM_H
#define TM_COM_H

#include <limits.h>

// Error macro; NOTE must be called with constant string as first arg
#define ERROR(...) ((void) fprintf(stderr, "ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) ((void) fprintf(stderr, "WARNING: " __VA_ARGS__))

/*
 * Definition of our symbol type. Using char allows us up to
 * 256 symbols, which lets us define 8-MMs at the highest.
 */
typedef unsigned char sym_t;
// NB. max sym bits should always fit in an int.
#define MAX_SYM_BITS ((int) (CHAR_BIT * sizeof (sym_t)))

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
#define STATE_UNDEF ('Z' - 'A')

// FUNCTION DEFS

void sym_bin_print(sym_t sym, unsigned sym_bits);
void print_state_and_sym(state_t state, sym_t sym, unsigned sym_bits, int directed);

#endif
