.PRECIOUS: bin/%.o

# Note that we don't use floats except for some debugging info, so ffast-math is okay
CFLAGS_RELEASE=-O3 -ffast-math -Wall -Wextra -std=c99
# Testing compiler flags
CFLAGS=-O0 -Wall -Wextra -std=c99 -g
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
		-readability-function-cognitive-complexity
LINT_FILTERS=$(LINT_INCL),$(LINT_EXCL)

COMMON=bin/tm.o

all: bin/test bin/vis

# static analysis
check: tm.c vis.c test.c
	# clang-check -analyze $^ -- $(CFLAGS)
	clang-tidy $^ -checks='$(LINT_FILTERS)' -- $(CFLAGS)

# dynamic analysis
test: bin/test
	bin/test
	# check for memory leaks, OSX specific and ANNOYING (codesign and other BS), just use valgrind instead if you can
	leaks --atExit -- $< -q

bin/%: bin/%.o $(COMMON)
	clang $(CFLAGS) $^ -o $@

bin/%.o: %.c
	@ mkdir -p bin/
	clang $(CFLAGS) -c $< -o $@

clean:
	rm -r bin/
