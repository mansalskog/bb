#ifndef TM_TEST_CASE_H
#define TM_TEST_CASE_H

struct test_case_t {
	char *txt;
	int steps;
	int nonzero;
};

extern const struct test_case_t TEST_CASES[];
extern const int N_TEST_CASES;

#endif
