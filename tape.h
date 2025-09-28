#ifndef TM_TAPE_H
#define TM_TAPE_H

/*
 * Definition of our symbol type. Using char allows us up to
 * 256 symbols, which lets us define 8-MMs at the highest.
 */
typedef unsigned char sym_t;
// NB. max sym bits should always fit in an int.
#define MAX_SYM_BITS ((int) (CHAR_BIT * sizeof (sym_t)))

/*
 * Holds a tape data object together with function pointers to its mutating functions ("methods").
 */
struct tape_t {
	void *data;
	void (*free)(struct tape_t *tape);
	sym_t (*read)(const struct tape_t *tape);
	void (*write)(struct tape_t *tape, sym_t sym);
	void (*move)(struct tape_t *tape, int delta);
};

int tape_cmp(struct tape_t *t1, struct tape_t *t2, int window);

#endif
