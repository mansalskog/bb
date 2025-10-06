#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Used only to get exit status of system(cmd) calls
#include <sys/wait.h>

/*
 * This file contains a "translator" (compiler, code generator) from the common Turing Machine
 * specification language into C. Later on we might add a translator to some assembly language,
 * but a modern C compiler should be faster than handwritten assembly, given that the generated C
 * is somewhat cleverly designed.
 */

#include "test_case.h"
#include "tm_def.h"

static const char *const COMPILE_CMD = "clang -O3 -g3 -Weverything -Wno-unsafe-buffer-usage -std=c99 -pedantic %s -o %s";
static const char *const RUN_CMD = "./%s";

static const int TAPE_SIZE = 100000;
static const int INIT_POS = TAPE_SIZE / 2;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
static void write_tabbed(FILE *out, int level, const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	while (level-- > 0)
		fprintf(out, "\t");
	vfprintf(out, fmt, args);
	va_end(args);
}
#pragma clang diagnostic pop

static void tm_write_instruction(const struct tm_instr_t instr, FILE *out)
{
	const int lvl = 3;
	write_tabbed(out, lvl, "tape[pos] = %d;\n", instr.sym);
	write_tabbed(out, lvl, "pos += %d;\n", instr.dir == DIR_RIGHT ? 1 : -1);
	write_tabbed(out, lvl, "goto state_%c;\n", instr.state + 'A');
}

/*
 * Generates a C file for the given TM definition.
 */
static void tm_gen_write(const struct tm_def_t *const def, const char *const src_file, const int safe)
{
	FILE *out = fopen(src_file, "w");
	write_tabbed(out, 0, "static int tape[%d] = {0};\n", TAPE_SIZE);
	write_tabbed(out, 0, "int main(int argc, char **argv)\n");
	write_tabbed(out, 0, "{\n");
	write_tabbed(out, 1, "int pos = %d;\n", INIT_POS);
	write_tabbed(out, 1, "int step = 0;\n", INIT_POS);
	write_tabbed(out, 1, "(void) argc; (void) argv; // Surpress unused\n");
	for (state_t i_state = 0; i_state < (state_t) def->n_states; i_state++) {
		write_tabbed(out, 0, "state_%c:\n", i_state + 'A');
		write_tabbed(out, 1, "step++;\n"); // TODO where should we actually place this instruction?
		if (safe) {
			write_tabbed(out, 1, "if (pos < 0 || pos >= %d)\n", TAPE_SIZE);
			write_tabbed(out, 2, "return -12345;\n");
		}
		write_tabbed(out, 1, "switch (tape[pos]) {\n");
		for (sym_t i_sym = 0; i_sym < (sym_t) def->n_syms; i_sym++) {
			const struct tm_instr_t instr = tm_def_lookup(def, i_state, i_sym);
			write_tabbed(out, 2, "case %d:\n", i_sym);
			tm_write_instruction(instr, out);
		}
		// ERROR HANDLING
		write_tabbed(out, 2, "default:\n");
		write_tabbed(out, 3, "return -12345;\n");
		// ERROR HANDLING
		write_tabbed(out, 1, "}\n");
	}
	// TODO handle other halt states than Z
	write_tabbed(out, 0, "state_Z:\n");
	write_tabbed(out, 1, "return step;\n");
	write_tabbed(out, 0, "}\n");
	fclose(out);
}

/*
 * Compiles the generated file.
 */
static void tm_gen_compile(const char *const src_file, const char *const bin_file)
{
	const size_t buflen = strlen(COMPILE_CMD) + strlen(src_file) + strlen(bin_file) + 1; // NB. a few extra chars
	char *buf = malloc(buflen);
	snprintf(buf, buflen, COMPILE_CMD, src_file, bin_file);
	buf[buflen - 1] = 0; // Ensure null termination
	int stat_val = system(buf);
	free(buf);
	if (!WIFEXITED(stat_val) || WEXITSTATUS(stat_val)) {
		ERROR("Compilation of TM program failed!\n");
	}
}

/*
 * Runs the compiled file and returns the number of steps taken.
 */
static int tm_gen_run(const char *const bin_file)
{
	const size_t buflen = strlen(RUN_CMD) + strlen(bin_file) + 1; // NB. a few extra chars
	char *buf = malloc(buflen);
	snprintf(buf, buflen, RUN_CMD, bin_file);
	int stat_val = system(buf); // NB. Can't be const due to WIFEXITED() etc
	free(buf);
	if (!WIFEXITED(stat_val)) {
		ERROR("Run of compiled TM failed!\n");
	}
	const int steps = WEXITSTATUS(stat_val);
	return steps;
}

static double verify_test_case(const struct test_case_t *const tcase, const int quiet)
{
	clock_t t = clock();
	struct tm_def_t *const def = tm_def_parse(tcase->txt);
	if (!quiet) printf("Parsed in %fs\n", seconds(clock(), t));

	if (!quiet) {
		printf("%s\n", tcase->txt);
		tm_def_print(def, 0);
	}

	const char *const dir = "./tmp/";
	const size_t fn_len = strlen(dir) + strlen(tcase->txt) + 3;
	char *const src_file = malloc(fn_len);
	snprintf(src_file, fn_len, "%s%s.c", dir, tcase->txt);
	char *const bin_file = malloc(fn_len);
	snprintf(bin_file, fn_len, "%s%s", dir, tcase->txt);

	t = clock();
	const int safe = 0;
	tm_gen_write(def, src_file, safe);
	if (!quiet) printf("Generated code in %fs\n", seconds(clock(), t));

	t = clock();
	tm_gen_compile(src_file, bin_file);
	if (!quiet) printf("Compiled code code in %fs\n", seconds(clock(), t));

	t = clock();
	const int steps = tm_gen_run(bin_file);
	const double runtime = seconds(clock(), t);
	if (!quiet) printf("Ran %d steps in %fs\n", steps, runtime);

	assert(steps == (tcase->steps & 0xFF)); // FIXME the exit status only gets the lowest byte, so we need to use program output instead...
	if (!quiet) printf("Test case is OK!\n");

	tm_def_free(def);
	free(src_file);
	free(bin_file);

	return runtime;
}

int main(const int argc, const char *const *const argv) {
	// More concise way to surpress "unused" warnings, #pragma is messy for clang
	(void) argc;
	(void) argv;

	int quiet = 1;

	double tot_runtime = 0.0;
	if (!quiet) printf("Verifying test cases...\n");
	for (int i = 0; i < N_TEST_CASES; i++) {
		tot_runtime += verify_test_case(TEST_CASES + i, quiet);
	}
	printf("Total runtime: %fs\n", tot_runtime);
	return 0;
}
