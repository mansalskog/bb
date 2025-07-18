#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <time.h>

#define DEBUG 0

#define MAX_STEPS (INT_MAX >> 4)

#define INIT_TAPE_LEN 1024
// Print this much either side of the tape head
#define PRINT_TAPE_CTX 5
// Print this much either side of the tape head
#define LOG_TAPE_CTX 20

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')
// Error macro, note must be called with constant string as first arg
#define ERROR(...) (printf("ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) (printf("WARNING: " __VA_ARGS__))

struct rle_t {
	struct rle_t *left;
	struct rle_t *right;
	char sym;
	int len;
};

struct tm_t {
	char *prog;
	int syms;
	int states;

	char *tape;
	int tape_len;

	int pos;
	char state;
	int step;
	int halt;

	struct rle_t *rle;
	int rle_pos;
};

// RLE functions

void rle_free(struct rle_t *rle) {
	while (rle->left) rle = rle->left;
	while (rle->right) {
		struct rle_t *next = rle->right;
		free(rle);
		rle = next;
	}
}

struct rle_t *rle_init() {
	struct rle_t *rle = malloc(sizeof *rle);
	rle->left = NULL;
	rle->right = NULL;
	rle->sym = '0';
	rle->len = 1;
	return rle;
}

void rle_print_run(char sym, int len, char sep) {
	if (len == 1) printf("%c%c", sym, sep);
	else printf("%c^%d%c", sym, len, sep);
}

void tm_rle_print(struct rle_t *rle, int rle_pos, char state) {
	struct rle_t *const orig = rle;
	while (rle->left) rle = rle->left;
	printf("..0 ");
	do {
		if (orig == rle) {
			if (rle_pos > 0) rle_print_run(rle->sym, rle_pos, '_');
			int right_len = rle->len - rle_pos - 1;
			printf("[%c]%c%c", rle->sym, state, right_len > 0 ? '_' : ' ');
			if (right_len > 0) rle_print_run(rle->sym, right_len, ' ');
		} else {
			rle_print_run(rle->sym, rle->len, ' ');
		}
		rle = rle->right;
	} while (rle);
	printf("0..");
	putchar('\n');
}

/* Writes to the current RLE, or the left or right, as appropriate.
 * Thus may modify the current RLE and the current position.
 */
void rle_write(struct rle_t **rle, int *rle_pos, char sym) {
	struct rle_t *const orig = *rle;
	if (orig->sym == sym) {
		// Nothing to do
		return;
	}

	if (*rle_pos == 0 && orig->left && orig->left->sym == sym) {
		// Extend left neighbor and shrink original
		*rle = orig->left;
		(*rle)->len++;
		*rle_pos = (*rle)->len - 1;

		orig->len--;
		if (orig->len == 0) {
			// Remove original
			if (orig->right) orig->right->left = orig->left;
			orig->left->right = orig->right;
			free(orig);
		}

		return;
	}

	if (*rle_pos == orig->len && orig->right && orig->right->sym == sym) {
		// Extend right neighbor and shrink original
		*rle = orig->right;
		(*rle)->len++;
		*rle_pos = 0;

		orig->len--;
		if (orig->len == 0) {
			// Remove original
			if (orig->left) orig->left->right = orig->right;
			orig->right->left = orig->left;
			free(orig);
		}
		return;
	}

	// Create new in middle / left / right
	struct rle_t *new_mid = malloc(sizeof *new_mid);
	new_mid->sym = sym;
	new_mid->len = 1;

	int left_len = *rle_pos;
	if (left_len > 0) {
		struct rle_t *new_left = malloc(sizeof *new_left);
		new_left->left = orig->left;
		new_left->right = new_mid;
		new_left->sym = orig->sym;
		new_left->len = left_len;

		new_mid->left = new_left;
		if (orig->left) orig->left->right = new_left;
	} else {
		new_mid->left = orig->left;
		if (orig->left) orig->left->right = new_mid;
	}

	int right_len = orig->len - *rle_pos - 1;
	if (right_len > 0) {
		struct rle_t *new_right = malloc(sizeof *new_right);
		new_right->left = new_mid;
		new_right->right = orig->right;
		new_right->sym = orig->sym;
		new_right->len = right_len;

		new_mid->right = new_right;
		if (orig->right) orig->right->left = new_right;
	} else {
		new_mid->right = orig->right;
		if (orig->right) orig->right->left = new_mid;
	}

	if (orig->left) assert(orig->left->right == new_mid || orig->left->right == new_mid->left);
	if (orig->right) assert(orig->right->left == new_mid || orig->right->left == new_mid->right);
	assert(new_mid->len == 1);
	assert(new_mid->sym == sym);

	*rle = new_mid;
	*rle_pos = 0;
	free(orig);
}

/* Moves in the RLEs by modifying the RLE pointer and the position.
 */
void rle_move(struct rle_t **rle, int *rle_pos, char dir) {
	struct rle_t *const orig = *rle;
	int dp = dir == 'L' ? -1 : 1;

	if (*rle_pos + dp < 0) {
		// Move off to the left
		if (orig->left == NULL) {
			orig->left = rle_init();
			orig->left->right = *rle;
		}
		*rle = orig->left;
		*rle_pos = (*rle)->len - 1;
		return;
	}

	if (*rle_pos + dp >= orig->len) {
		// Move off to the right
		if (orig->right == NULL) {
			orig->right = rle_init();
			orig->right->left = *rle;
		}
		*rle = orig->right;
		*rle_pos = 0;
		return;
	}

	// Move within the RLE
	*rle_pos += dp;
	return;
}

int rle_nonzero(struct rle_t *rle) {
	struct rle_t *const orig = rle;
	int nonzero = 0;
	while (rle) {
		if (rle->sym != '0') nonzero += rle->len;
		rle = rle->left;
	}
	rle = orig->right; // Already counted orig
	while (rle) {
		if (rle->sym != '0') nonzero += rle->len;
		rle = rle->right;
	}
	return nonzero;
}

// For debugging
void rle_tape_compare(struct tm_t *tm) {
	struct rle_t *rle = tm->rle;

	// Compare current RLE going left
	int pos = tm->pos;
	for (int i = tm->rle_pos; i >= 0; i--, pos--) {
		if (tm->tape[pos] != rle->sym) {
			goto fail;
		}
	}

	// Compare to the left
	rle = rle->left;
	while (rle) {
		for (int i = rle->len - 1; i >= 0; i--, pos--) {
			if (tm->tape[pos] != rle->sym) {
				goto fail;
			}
		}
		rle = rle->left;
	}

	// Compare current RLE going right
	rle = tm->rle;
	pos = tm->pos + 1;
	for (int i = tm->rle_pos + 1; i < rle->len; i++, pos++) {
		if (tm->tape[pos] != rle->sym) {
			goto fail;
		}
	}

	// Compare to the right
	rle = rle->right;
	while (rle) {
		for (int i = 0; i < rle->len; i++, pos++) {
			if (tm->tape[pos] != rle->sym) {
				goto fail;
			}
		}
		rle = rle->right;
	}

	return;

	int print_to, print_from; // Strange C99 quirk
fail:
	print_to = pos + 3;
	if (print_to >= tm->tape_len) print_to = tm->tape_len;
	print_from = pos - 2;
	if (print_from < 0) print_from = 0;
	printf("RLE-tape comparison failed at %d with tape %.*s and RLE %c^%d %c^%d %c^%d\n",
		pos,
		print_to - print_from, tm->tape + print_from,
		rle->left ? rle->left->sym : '0', rle->left ? rle->left->len : -1,
		rle->sym, rle->len,
		rle->right ? rle->right->sym : '0', rle->right ? rle->right->len : -1);
	exit(1);
}

// TM functions

void tm_free(struct tm_t *tm) {
	free(tm->prog);
	free(tm->tape);
	free(tm);
}

/* Parses a Turing Machine from a given standard text format
 * e.g. "1RB1LB_1LA1LZ"
 */
struct tm_t *tm_parse(char *txt) {
	struct tm_t *tm = malloc(sizeof *tm);

	int cols = 0;
	while(txt[cols] && txt[cols] != '_') cols++;

	if (cols % 3 != 0) {
		ERROR("Invalid width %d of row, should be divisible by 3.\n", cols);
	}
	tm->syms = cols / 3;

	int len = strlen(txt);
	// Each row contains three characters per sym, and one underscode for each except the last row
	// We check that the string is well-formed below, so no length check here
	tm->states = (len + 1) / (tm->syms * 3 + 1);
	tm->prog = malloc(tm->states * tm->syms * 3);

	for (int i = 0; i < tm->states; i++) {
		for (int j = 0; j < tm->syms; j++) {
			int b = i * (tm->syms * 3 + 1) + j * 3; // Index into txt
			int bp = (i * tm->syms + j) * 3; // Index into tm->prog
			char sym = txt[b];
			if (!sym || sym < '0' || sym - '0' >= tm->syms) {
				// Allow unused states only if all three chars are '-'
				// NB. short-circuiting prevents reading past end of string here
				if (sym == '-' && txt[b + 1] == '-' && txt[b + 2] == '-') {
					tm->prog[bp] = '-';
					tm->prog[bp + 1] = '-';
					tm->prog[bp + 2] = '-';
					continue;
				}
				ERROR("Invalid symbol %c at row %d col %d, should be 0-%c.\n",
					ONLY_ALNUM(sym), i, j, '0' + tm->syms - 1);
			}
			char dir = txt[b + 1];
			if (!dir || (dir != 'L' && dir != 'R')) {
				ERROR("Invalid direction %c at row %d col %d.\n",
					ONLY_ALNUM(dir), i, j);
			}
			char state = txt[b + 2];
			if (!state || state < 'A' || state - 'A' >= tm->states) {
				if (state != 'Z' && state != 'H') {
					WARN("Unusual halting state state %c at row %d col %d, should be A-%c.\n",
						ONLY_ALNUM(state), i, j, 'A' + tm->states - 1);
				}
			}
			// Save the program
			tm->prog[bp] = sym;
			tm->prog[bp + 1] = dir;
			tm->prog[bp + 2] = state;
		}
		char uscore = txt[i * (tm->syms * 3 + 1) + tm->syms * 3];
		if (uscore != '_') {
			if (uscore != 0 || i != tm->states - 1) {
				ERROR("Invalid row terminator %c at row %d, should be underscore.\n",
					ONLY_ALNUM(uscore), i);
			}
		}
	}

	// Setup initial tape
	tm->tape_len = INIT_TAPE_LEN;
	tm->tape = malloc(tm->tape_len);
	memset(tm->tape, '0', tm->tape_len);
	tm->pos = tm->tape_len / 2;

	// Initial position and state
	tm->state = 'A';
	tm->step = 0;
	tm->halt = 0;

	tm->rle = rle_init();
	tm->rle_pos = 0;

	return tm;
}

/* Prints the program of the given TM as a table.
 */
void tm_print_prog(struct tm_t *tm) {
	printf(" ");
	for (int j = 0; j < tm->states; j++) {
		printf(" %d  ", j + 1);
	}
	for (int i = 0; i < tm->syms; i++) {
		printf("\n%c ", 'A' + i);
		for (int j = 0; j < tm->states; j++) {
			printf("%.*s ", 3, tm->prog + (i * tm->states + j) * 3);
		}
	}
}

/* Prints an excerpt of the tape with PRINT_TAPE_CTX symbols on
 * either side of the head.
 */
void tm_print_tape(FILE *f, struct tm_t *tm, int ctx) {
	int left = tm->pos - ctx;
	left = left < 0 ? 0 : left;
	int right = tm->pos + ctx;
	right = right >= tm->tape_len ? tm->tape_len - 1 : right;
	fprintf(f, "%.*s [%c]%c %.*s\n",
			tm->pos - left, tm->tape + left,
			tm->tape[tm->pos], tm->state,
			right - tm->pos, tm->tape + tm->pos + 1);
}

/* Moves left or right in the TM, reallocating tape as required.
 * dir is given as 'L' or 'R'
 */
void tm_move(struct tm_t *tm, char dir) {
	int dp = dir == 'L' ? -1 : 1;

	if (tm->pos + dp < 0 || tm->pos + dp >= tm->tape_len) {
		// Ran out of tape, allocate more
		int len = tm->tape_len;
		char *tape = malloc(len * 2);
		// Place data in the "middle" of the tape, growing it by 50% on both sides
		memset(tape, '0', len / 2);
		memset(tape + len / 2 * 3, '0', len / 2);
		memcpy(tape + len / 2, tm->tape, len);
		tm->tape_len = len * 2;
		free(tm->tape);
		tm->tape = tape;
		tm->pos += len / 2;
	}

	tm->pos += dp;
}

int tm_step(struct tm_t *tm) {
	if (tm->halt) {
		ERROR("Trying to step halted TM.\n");
	}

	// Read the symbol
	char in_sym = tm->tape[tm->pos];

	int ip = (tm->state - 'A') * (tm->syms * 3) + (in_sym - '0') * 3;

	char out_sym = tm->prog[ip];
	char out_dir = tm->prog[ip + 1];
	char out_state = tm->prog[ip + 2];

	// Tape write and move
	tm->tape[tm->pos] = out_sym;
	tm_move(tm, out_dir);

	// RLE write and move
	DEBUG && printf("RLE before write: ");
	if (DEBUG) tm_rle_print(tm->rle, tm->rle_pos, tm->state);

	rle_write(&tm->rle, &tm->rle_pos, out_sym);
	DEBUG && printf("RLE after write: ");
	if (DEBUG) tm_rle_print(tm->rle, tm->rle_pos, tm->state);

	rle_move(&tm->rle, &tm->rle_pos, out_dir);
	DEBUG && printf("RLE after move: ");
	if(DEBUG) tm_rle_print(tm->rle, tm->rle_pos, tm->state);

	tm->state = out_state;
	tm->step++;

	if (out_state >= 'A' + tm->states) {
		tm->halt = 1;
	}
	return tm->halt;
}

struct tm_t *tm_run(char *txt) {
	struct tm_t *tm = tm_parse(txt);
	while (tm->step < MAX_STEPS) {
		int halt = tm_step(tm);
		rle_tape_compare(tm); // SLOW
		if (halt) break;
	}
	return tm;
}

struct tm_t *tm_run_vis(char *txt) {
	struct tm_t *tm = tm_parse(txt);
	while (tm->step < MAX_STEPS) {
		tm_print_tape(stdout, tm, PRINT_TAPE_CTX);
		tm_rle_print(tm->rle, tm->rle_pos, tm->state);
		rle_tape_compare(tm);
		// printf("Enter to continue\n");
		// getchar();
		int halt = tm_step(tm);
		if (halt) break;
	}
	return tm;
}

// Slow way to count number of non-zero symbols in tape
int tm_nonzero(struct tm_t *tm) {
	int nonzero = 0;
	for (int i = 0; i < tm->tape_len; i++) {
		if (tm->tape[i] != '0') nonzero++;
	}
	return nonzero;
}

double seconds(clock_t t1, clock_t t0) {
	return ((double) (t1 - t0)) / CLOCKS_PER_SEC;
}

struct run_t {
	char *txt;
	int steps;
	int nonzero;
};

void verify() {
	struct run_t runs[] = {
		// BB(3)
		{"1RB1RZ_1LB0RC_1LC1LA", 21, 5},
		{"1RB1RZ_0LC0RC_1LC1LA", 20, 5},
		{"1RB1LA_0RC1RZ_1LC0LA", 20, 5},
		{"1RB1RA_0RC1RZ_1LC0LA", 19, 5},
		{"1RB0RA_0RC1RZ_1LC0LA", 19, 4},
		{"1RB0LC_1LA1RZ_1RC1RB", 18, 5},
		{"1RB0LB_0RC1RC_1LA1RZ", 18, 5},
		{"1RB1LB_0RC1RZ_1LC0LA", 18, 4},
		{"1RB0LB_0RC1RZ_1LC0LA", 18, 3},
		{"1RB1RZ_0RC0RC_1LC1LA", 17, 5},
		{"1RB1RZ_0RC---_1LC0LA", 17, 4},
		{"1RB1LC_0LC1RA_1RZ1LA", 16, 5},
		{"1RB0LC_1LC1RB_1RZ1LA", 16, 5},
		{"1RB1LA_0LA1RC_1RA1RZ", 15, 5},
		{"1RB1LA_0LA1RC_0RB1RZ", 15, 4},
		{"1RB0LC_1RC1RZ_1LA0RB", 15, 4},
		{"1RB0RB_0LC1RZ_1RA1LC", 15, 3},
		{"1RB1RZ_0RC1RB_1LC1LA", 14, 6},
		{"1RB1RZ_1LB0LC_1RA1RC", 14, 5},
		{"1RB1RZ_0LC1RA_1LA1LB", 14, 5},
		{"1RB1RZ_1LC1RA_1RC0LB", 14, 4},
		{"1RB1RZ_1LB0LC_1RA0RC", 14, 3},
		{"1RB1RZ_0LC0RB_1LA1LC", 14, 3},
		{"1RB0RB_1LC1RZ_0LA1RA", 14, 3},
		{"1RB1RZ_0LC0RA_1RA1LB", 14, 2},
		// BB(2,3)
		{"1RB2LB1RZ_2LA2RB1LB", 38, 9},
		{"1RB0LB1RZ_2LA1RB1RA", 29, 8},
		{"1RB2LA1RZ_1LB1LA0RA", 26, 6},
		{"1RB1LA1LB_0LA2RA1RZ", 26, 6},
		{"1RB1LB1RZ_2LA2RB1LB", 24, 7},
		{"1RB2LA1RZ_0LB1LA0RA", 22, 4},
		{"1RB2RB1RZ_1LB1RA0LA", 21, 4},
		{"1RB2LA2RB_1LA1RZ1RA", 20, 6},
		{"1RB2LA1RZ_2LA2RB1LB", 20, 6},
		{"1RB2LA0RB_1LA1RZ1RA", 20, 5},
		{"1RB0LB1RZ_2LA2LB1RA", 19, 7},
		{"1RB0RB1RZ_1LB2RA1LA", 19, 6},
		{"1RB2LB1LA_2LA1RZ2RB", 18, 7},
		{"1RB1LB2LA_1LA2RB1RZ", 18, 7},
		{"1RB2LB2LA_2LA1RZ0RA", 18, 6},
		{"1RB2LA1RZ_1LB1RA2RB", 18, 6},
		{"1RB2LB1RZ_2LA1RA0LB", 18, 5},
		{"1RB0RB1RZ_1LB2LA2RA", 17, 6},
		{"1RB2LB0LB_2LA1RZ2RB", 17, 4},
		{"1RB2RA1RZ_0LB2RB1LA", 17, 3},
		// BB(2,4)
		{"1RB2LA1RA1RA_1LB1LA3RB1RZ", 3932964, 2050},
		{"1RB3LA1LA1RA_2LA1RZ3RA3RB", 7195, 90},
		{"1RB3LA1LA1RA_2LA1RZ3LA3RB", 6445, 84},
		{"1RB3LA1LA1RA_2LA1RZ2RA3RB", 6445, 84},
		{"1RB2RB3LA2RA_1LA3RB1RZ1LB", 2351, 60},
		{"1RB3RA1LA2RB_2LA3LA1RZ1RA", 1021, 53},
		{"1RB3LA1RZ0RB_2LB2LA0LA0RA", 1001, 26},
		{"1RB2LA1RZ3LA_2LA2RB3RB2LB", 770, 30},
		{"1RB2LB1RZ3LA_2LA2RB3RB2LB", 708, 29},
		{"1RB2LA0RB1LB_1LA3RA1RA1RZ", 592, 24},
		{"1RB3LA3RA0LA_2LB1LA1RZ2RA", 392, 20},
		{"1RB3LA1LA1RA_2LA1RZ2RB3RB", 376, 21},
		{"1RB3LA1LA1RA_2LA1RZ0LA3RB", 376, 21},
		{"1RB3LA3RB0LA_2LA1RZ1LB3RA", 374, 22},
		{"1RB1RA1LB0RA_2LA3RB2RA1RZ", 335, 18},
		{"1RB2LA3LA1LB_2LA1RZ2RB0RA", 292, 18},
		{"1RB0RB1RZ0LA_2LA3RA3LA1LB", 289, 13},
		{"1RB1LA1RZ0LB_2LA3RB1RB2RA", 283, 18},
		{"1RB3LA3RA0LA_2LB1LA1RZ3RA", 266, 19},
		{"1RB2RB1RZ1LB_2LA2LB3RB0RB", 241, 15},
		// BB(5)
		{"1RB1LC_1RC1RB_1RD0LE_1LA1LD_1RZ0LA", 47176870, 4098},
		{"1RB0LD_1LC1RD_1LA1LC_1RZ1RE_1RA0RB", 23554764, 4097},
		{"1RB1RA_1LC1LB_1RA0LD_0RB1LE_1RZ0RB", 11821234, 4097},
		{"1RB1RA_1LC1LB_1RA0LD_1RC1LE_1RZ0RB", 11821220, 4097},
		{"1RB1RA_0LC0RC_1RZ1RD_1LE0LA_1LA1LE", 11821190, 4096},
		{"1RB1RA_1LC0RD_1LA1LC_1RZ1RE_1LC0LA", 11815076, 4096},
		{"1RB1RA_1LC1LB_1RA0LD_0RB1LE_1RZ1LC", 11811040, 4097},
		{"1RB1RA_1LC1LB_0RC1LD_1RA0LE_1RZ1LC", 11811040, 4097},
		{"1RB1RA_1LC1LB_1RA0LD_1RC1LE_1RZ1LC", 11811026, 4097},
		{"1RB1RA_0LC0RC_1RZ1RD_1LE1RB_1LA1LE", 11811010, 4096},
		{"1RB1RA_1LC1LB_1RA1LD_0RE0LE_1RZ1LC", 11804940, 4097},
		{"1RB1RA_1LC1LB_1RA1LD_1RA0LE_1RZ1LC", 11804926, 4097},
		{"1RB1RA_1LC0RD_1LA1LC_1RZ1RE_0LE1RB", 11804910, 4096},
		{"1RB1RA_1LC0RD_1LA1LC_1RZ1RE_1LC1RB", 11804896, 4096},
		{"1RB1RA_1LC1LB_1RA1LD_1RA1LE_1RZ0LC", 11798826, 4098},
		{"1RB1RA_1LC1RD_1LA1LC_1RZ0RE_1LC1RB", 11798796, 4097},
		{"1RB1RA_1LC1RD_1LA1LC_1RZ1RE_0LE0RB", 11792724, 4097},
		{"1RB1RA_1LC1RD_1LA1LC_1RZ1RE_1LA0RB", 11792696, 4097},
		{"1RB1RA_1LC1RD_1LA1LC_1RZ1RE_1RA0RB", 11792682, 4097},
		{"1RB1RZ_1LC1RC_0RE0LD_1LC0LB_1RD1RA", 2358064,  1471},
	};
	int n_runs = sizeof runs / sizeof *runs;

	double tot = 0.0;
	for (int i = 0; i < n_runs; i++) {
		clock_t t = clock();
		struct tm_t *tm = tm_run(runs[i].txt);
		assert(runs[i].steps == tm->step);
		assert(runs[i].nonzero == tm_nonzero(tm));
		assert(runs[i].nonzero == rle_nonzero(tm->rle));
		tot += seconds(clock(), t);
	}
	printf("Total time: %fs\n", tot);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		// printf("Usage: %s [tm]\n", argv[0]);
		verify();
		return 1;
	}

	// char *test = "1RB1RZ_0LC0RA_1RA1LB";
	tm_run_vis(argv[1]);

	return 0;
}
