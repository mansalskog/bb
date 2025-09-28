#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "tape.h"
#include "tape_flat.h"
#include "tape_rle.h"
#include "tape_bit.h"
#include "test_case.h"
#include "tm_def.h"
#include "tm_run.h"
#include "util.h"

// Upper limit on number of steps. NB not a hard limit, may be exceeded by up to BATCH_STEPS - 1.
static const int MAX_STEPS = (INT_MAX >> 4);
// Number of steps before we perform some consistency checks
static const int BATCH_STEPS = 100;
// Number of symbols in each direction of the head that we compare. 0 means comparing only head
static const int COMPARE_WINDOW = 1000;

static double seconds(const clock_t t1, const clock_t t0)
{
	return ((double) (t1 - t0)) / CLOCKS_PER_SEC;
}

struct flags_t {
	unsigned quiet : 1;
	unsigned compare : 1;
	unsigned tape_rle : 1;
	unsigned tape_flat : 1;
	unsigned tape_bit : 1;
};

static double verify_test_case(const struct test_case_t *const tcase, const struct flags_t flags)
{
	clock_t t = clock();
	struct tm_def_t *const def = tm_def_parse(tcase->txt);
	if (!flags.quiet) printf("Parsed in %fs\n", seconds(clock(), t));

	if (!flags.quiet) {
		printf("%s\n", tcase->txt);
		tm_def_print(def, 0);
	}

	t = clock();
	const unsigned sym_bits = ceil_log2((unsigned) def->n_syms);

	struct tape_t *const rle_tape = flags.tape_rle ? rle_tape_init(sym_bits) : 0;

	const int flat_tape_len = 16;
	const int flat_tape_origin = flat_tape_len / 2;
	struct tape_t *const flat_tape = flags.tape_flat ? flat_tape_init(sym_bits, flat_tape_len, flat_tape_origin) : 0;

	const int bit_tape_len = 50000; // More than enough for our test cases
	const int bit_tape_origin = bit_tape_len / 2;
	struct tape_t *const bit_tape = flags.tape_bit ? bit_tape_init(sym_bits, bit_tape_len, bit_tape_origin) : 0;

	struct tm_run_t *const run = tm_run_init(def, rle_tape, flat_tape, bit_tape);
	if (!flags.quiet) printf("Initialized in %fs\n", seconds(clock(), t));

	t = clock();
	while (1) {
		tm_run_steps(run, BATCH_STEPS);

		if (flags.compare) {
			if (flat_tape && rle_tape) {
				const int cmp = tape_cmp(flat_tape, bit_tape, COMPARE_WINDOW);
				assert(!cmp);
				printf("Flat and RLE comparison %s!\n", cmp ? "FAILED": "OK");
			}
			if (rle_tape && bit_tape) {
				const int cmp = tape_cmp(rle_tape, bit_tape, COMPARE_WINDOW);
				assert(!cmp);
				printf("RLE and bit comparison %s!\n", cmp ? "FAILED" : "OK");
			}
			if (bit_tape && flat_tape) {
				const int cmp = tape_cmp(bit_tape, flat_tape, COMPARE_WINDOW);
				assert(!cmp);
				printf("Bit and flat comparison %s!\n", cmp ? "FAILED" : "OK");
			}
		}

		if (tm_run_halted(run))
			break;
		if (run->steps >= MAX_STEPS)
			break;
	}
	const double runtime = seconds(clock(), t);
	if (!flags.quiet) printf("Ran %d steps in %fs\n", run->steps, runtime);

	assert(tm_run_halted(run));

	assert(run->steps == tcase->steps);
	if (!flags.quiet) printf("Test case is OK!\n");

	tm_run_free(run);
	tm_def_free(def);
	if (rle_tape)
		rle_tape->free(rle_tape);
	if (flat_tape)
		flat_tape->free(flat_tape);
	if (bit_tape)
		bit_tape->free(bit_tape);

	return runtime;
}

static void unknown_argument(const char *arg0, const char *arg)
{
	(void) fprintf(stderr, "Unknown argument '%s'.\n", arg);
	(void) fprintf(stderr, "Usage: %s [-q]\n", arg0);
	(void) fprintf(stderr, "\t-q\tQuiet, print no output.\n");
}

int main(int argc, char **argv)
{
	struct flags_t flags = {0};
	for (int i = 1; i < argc; i++) {
		size_t len = strlen(argv[i]);
		if (len != 2 || argv[i][0] != '-') {
			unknown_argument(argv[0], argv[i]);
			return 1;
		}
		switch (argv[i][1]) {
		case 'b':
			flags.tape_bit = 1;
			break;
		case 'c':
			flags.compare = 1;
			break;
		case 'f':
			flags.tape_flat = 1;
			break;
		case 'r':
			flags.tape_rle = 1;
			break;
		case 'q':
			flags.quiet = 1;
			break;
		default:
			unknown_argument(argv[0], argv[i]);
			return 1;
		}
	}

	if (flags.compare && (flags.tape_flat + flags.tape_rle + flags.tape_bit < 2)) {
		ERROR("Must use at least two tapes to enable comparison!\n");
	}

	// Temporary, delete later
	bit_tape_test();

	// Run test cases 10 times for benchmarking
	double tot_runtime = 0.0;
	for (int j = 0; j < 10; j++) {
		if (!flags.quiet) printf("Verifying test cases...\n");
		for (int i = 0; i < N_TEST_CASES; i++) {
			tot_runtime += verify_test_case(TEST_CASES + i, flags);
		}
	}
	printf("Total runtime: %fs Using tapes: %s %s %s\n", tot_runtime, flags.tape_flat ? "flat" : "", flags.tape_rle ? "RLE" : "", flags.tape_bit ? "bitarray" : "");
	return 0;
}
