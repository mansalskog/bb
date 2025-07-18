#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define INIT_TAPE_LEN 1024
// Print this much either side of the tape head
#define PRINT_TAPE_CTX 5
// Print this much either side of the tape head
#define LOG_TAPE_CTX 20

#define ONLY_ALNUM(c) ((c) != 0 ? (c) : '?')
// Error macro, note must be called with constant string as first arg
#define ERROR(...) (printf("ERROR: " __VA_ARGS__), exit(1))
#define WARN(...) (printf("WARNING: " __VA_ARGS__))

struct tm_t {
	char *prog;
	int syms;
	int states;

	char *tape;
	int tape_len;

	int pos;
	char state;
	int step;
};

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

	printf("Reading program with %d symbols and %d states.\n", tm->syms, tm->states);
	for (int i = 0; i < tm->states; i++) {
		for (int j = 0; j < tm->syms; j++) {
			int b = i * (tm->syms * 3 + 1) + j * 3; // Index into txt
			printf("%d\n", b);
			int bp = (i * tm->syms + j) * 3; // Index into tm->prog
			char sym = txt[b];
			if (!sym || sym < '0' || sym - '0' >= tm->syms) {
				// Allow unused states only if all three chars are '-'
				// NB. short-circuiting prevents reading past end of string here
				if (sym == '-' && txt[b + 1] == '-' && txt[b + 2] == '-') {
					WARN("Unused state");
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
		memcpy(tape + len / 2, tm->tape, len);
		tm->tape_len = len * 2;
		free(tm->tape);
		tm->tape = tape;
	}

	tm->pos += dp;
}

int tm_step(struct tm_t *tm) {
	// Read the symbol
	char in_sym = tm->tape[tm->pos];

	int ip = (tm->state - 'A') * (tm->syms * 3) + (in_sym - '0') * 3;

	char out_sym = tm->prog[ip];
	char out_dir = tm->prog[ip + 1];
	char out_state = tm->prog[ip + 2];

	tm->tape[tm->pos] = out_sym;
	tm_move(tm, out_dir);
	tm->state = out_state;
	tm->step++;

	return out_state >= 'A' + tm->states;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [tm]\n", argv[0]);
		return 1;
	}

	FILE *f = fopen("log", "w");

	struct tm_t *tm = tm_parse(argv[1]);
	tm_print_prog(tm);
	tm_print_tape(stdout, tm, PRINT_TAPE_CTX);
	while (1) {
		int halt = tm_step(tm);
		tm_print_tape(stdout, tm, PRINT_TAPE_CTX);
		tm_print_tape(f, tm, LOG_TAPE_CTX);
		puts("Enter to continue");
		getchar();
		if (halt) break;
	}

	free(tm);
	fflush(f);
	fclose(f);

	return 0;
}
