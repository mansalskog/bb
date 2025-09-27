.PRECIOUS: bin/%.o

# Common compiler flags, should not be used by themselves
_CFLAGS=-std=c99 -ferror-limit=0 -Wall -Wextra -Wconversion -Wdouble-promotion
# Debugging symbols, no sanitizers to allow the "leaks" utility to run
CFLAGS_DEBUG=$(_CFLAGS) -O3 -g3
# Optimized build to match release, but still with sanitization for testing
CFLAGS_TEST=$(_CFLAGS) -O3 -ffast-math -fsanitize=address,undefined,integer
# "Release" build, as fast as possible
# Note that we don't use floats except for some debugging info, so ffast-math is okay
CFLAGS_RELEASE=$(_CFLAGS) -O3 -ffast-math -DNDEBUG

# Include as many linter checks as possible
LINT_INCL=bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,hicpp-*,linuxkernel-*,llvm-*,misc-*,performance-*,portability-*,readability-*
# Disabled some checks because:
# bugprone-easily-swappable-parameters			This is too much work to "fix", and most functions have a logical paramter order.
# readability-identifier-length					I prefer maximum(a, b) to maximum(num1, num2).
# readability-math-missing-parentheses			I know PEMDAS.
# *-braces-around-statements					I use K&R coding style.
# readability-function-cognitive-complexity		I don't like to split up functions unneccesarily
LINT_EXCL=\
		-bugprone-easily-swappable-parameters,\
		-readability-identifier-length,\
		-readability-math-missing-parentheses,\
		-*-braces-around-statements,\
		-readability-function-cognitive-complexity,\
LINT_FILTERS=$(LINT_INCL),$(LINT_EXCL)

SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)

COMMON_C=tm_com.c tm_run.c tm_def.c tape_flat.c tape_rle.c util.c

all: test

# static analysis of all source files
check: $(SOURCES) $(HEADERS)
	# clang-check -analyze $^ -- $(CFLAGS_DEBUG)
	clang-tidy $^ -checks='$(LINT_FILTERS)' -- $(CFLAGS_DEBUG)

# dynamic analysis
test: bin/tst_test bin/dbg_test
	# run test with sanitizers
	bin/tst_test
	# check for memory leaks, OSX specific and ANNOYING (codesign and other BS), just use valgrind instead if you can
	# run memory leak checker without address sanitizer
	# disabled due to the tool being stupid and useless "unable to inspect heap ranges of target process; it may be using a malloc replacement library without the required support"
	# leaks --atExit -- bin/dbg_test -q

bin/rel_%: %.c $(COMMON_C)
	@ mkdir -p bin/
	clang $(CFLAGS_RELEASE) $^ -o $@

bin/tst_%: %.c $(COMMON_C)
	@ mkdir -p bin/
	clang $(CFLAGS_TEST) $^ -o $@

bin/dbg_%: %.c $(COMMON_C)
	@ mkdir -p bin/
	clang $(CFLAGS_DEBUG) $^ -o $@

clean:
	rm -r bin/
