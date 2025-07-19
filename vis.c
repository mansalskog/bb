#include <stdio.h>

#include "tm.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s [tm]\n", argv[0]);
		return 1;
	}

	struct tm_def_t *const def = tm_def_parse(argv[1]);
	struct tm_run_t *const run = tm_run_init(def);

	while (1) {
		tm_run_print_tape(run);
		tm_run_step(run);
		(void) getchar();
	}

	return 0;
}
