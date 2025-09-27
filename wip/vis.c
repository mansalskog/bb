#include <stdio.h>

#include "tm.h"

int main(int argc, char **argv)
{
	/*
	if (argc < 2) {
		printf("Usage: %s [tm]\n", argv[0]);
		return 1;
	}
	*/

	const char *test_txt = "1RB1RZ_1LB0RC_1LC1LA";
	struct tm_def_t *const tm_def = tm_def_parse(test_txt);
	tm_def_print(tm_def, 0);

	struct tm_def_t *const mm_def = tm_def_to_mm_def(tm_def, 2);
	tm_def_print(mm_def, 1);

	struct tm_run_t *const tm_run = tm_run_init(tm_def, 1, 16, 8);
	struct tm_run_t *const mm_run = tm_run_init(mm_def, 1, 16, 8);

	while (1) {
		tm_run_print_tape(mm_run, 0);
		tm_run_step(mm_run);
		while (1) {
			tm_run_print_tape(tm_run, 0);
			tm_run_step(tm_run);
			// FIXME should just compare flat tapes when that is implemented
			// FIXME this fails because they have different symbol bits...
			int cmp = tm_mixed_tape_cmp(mm_run->rle_tape, tm_run->flat_tape);
			if (cmp == TAPES_EQUAL)
				break;
		}
		(void) getchar();
	}

	return 0;
}
