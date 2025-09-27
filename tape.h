#ifndef TM_TAPE_H
#define TM_TAPE_H

/*
 * Holds a tape data object together with function pointers to its mutating functions ("methods").
 */
struct tape_t {
	void *tape;
	void (*free)(void *tape);
	sym_t (*read)(const void *tape);
	void (*write)(void *tape, sym_t sym);
	void (*move)(void *tape, int delta);
};

#endif
