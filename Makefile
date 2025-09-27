.PRECIOUS: bin/%.o

# Common compiler flags, should not be used by themselves
_CFLAGS=-Wall -Wextra -std=c99 -ferror-limit=0
# Note that we don't use floats except for some debugging info, so ffast-math is okay
CFLAGS_RELEASE=-O3 -ffast-math -DNDEBUG $(_CFLAGS)
# Testing compiler flags
CFLAGS_TEST=-O0 -g $(_CFLAGS)
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

COMMON_C=tm_com.c tm_run.c tm_def.c tape_flat.c tape_rle.c util.c
SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)

fast: bin/fast

bin/fast: test.c $(COMMON_C)
	@ mkdir -p bin/
	clang $(CFLAGS_RELEASE) $^ -o $@

# static analysis
check: $(SOURCES) $(HEADERS)
	# clang-check -analyze $^ -- $(CFLAGS_TEST)
	clang-tidy $^ -checks='$(LINT_FILTERS)' -- $(CFLAGS_TEST)

# dynamic analysis
test: bin/test
	bin/test
	# check for memory leaks, OSX specific and ANNOYING (codesign and other BS), just use valgrind instead if you can
	leaks --atExit -- $< -q

bin/%: %.c $(COMMON_C)
	@ mkdir -p bin/
	clang $(CFLAGS_TEST) $^ -o $@

clean:
	rm -r bin/
