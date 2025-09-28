#ifndef TM_UTIL_H
#define TM_UTIL_H

#include <stdlib.h>

#include "tape.h"

// Error macro; NOTE must be called with constant string as first arg
#define ERROR(...) ((void) fprintf(stderr, "ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) ((void) fprintf(stderr, "WARNING: " __VA_ARGS__))

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

int maximum(int a, int b);
unsigned ceil_log2(unsigned n);
unsigned long bitmask(unsigned from, unsigned to);
void print_bin(const char *pre, unsigned long val);

#ifndef NDEBUG
void assert_eq(unsigned long a, unsigned long b);
#endif

void sym_bin_print(sym_t sym, unsigned sym_bits);
void print_state_and_sym(state_t state, sym_t sym, unsigned sym_bits, int directed);

#endif
