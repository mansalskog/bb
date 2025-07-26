// Common TM definitions
#ifndef TM_COM_H
#define TM_COM_H

// Error macro; NOTE must be called with constant string as first arg
#define ERROR(...) ((void) fprintf(stderr, "ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) ((void) fprintf(stderr, "WARNING: " __VA_ARGS__))

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

#endif
