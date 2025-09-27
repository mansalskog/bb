#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h>

// Error macro; NOTE must be called with constant string as first arg
#define ERROR(...) ((void) fprintf(stderr, "ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) ((void) fprintf(stderr, "WARNING: " __VA_ARGS__))

#define STATE_UNDEF ('Z' - 'A')

int maximum(int a, int b);
unsigned ceil_log2(unsigned n);
unsigned long bitmask(unsigned from, unsigned to);
void print_bin(const char *pre, unsigned long val);

#ifndef NDEBUG
void assert_eq(unsigned long a, unsigned long b);
#endif

#endif
