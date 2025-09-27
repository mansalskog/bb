.PRECIOUS: bin/%.o

# Note that we don't use floats except for some debugging info, so ffast-math is okay
CFLAGS_RELEASE=-O3 -ffast-math -Wall -Wextra -std=c99
# Testing compiler flags
CFLAGS=-O0 -Wall -Wextra -std=c99 -g -ferror-limit=0
# Include as many linter checks as possible
LINT_INCL=bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,hicpp-*,linuxkernel-*,llvm-*,misc-*,performance-*,portability-*,readability-*
# Disabled some checks because:
# bugprone-easily-swappable-parameters			This is too much work to "fix", and most functions have a logical paramter order.
# readability-identifier-length					I prefer maximum(a, b) to maximum(num1, num2).
# readability-math-missing-parentheses			I know PEMDAS.
# *-braces-around-statements					I use K&R coding style.
# readability-function-cognitive-complexity		I don't like to split up functions unneccesarily
# insecureAPI.rand								The rand function is used for testing only
# cert-msc30-c									The rand function is used for testing only
# cert-msc50-cpp								The rand function is used for testing only
# cert-msc32-c									The rand function is used for testing only
# cert-msc51-cpp								The rand function is used for testing only
# hicpp-signed-bitwise							Shifting with signed ints is better
LINT_EXCL=\
		-bugprone-easily-swappable-parameters,\
		-readability-identifier-length,\
		-readability-math-missing-parentheses,\
		-*-braces-around-statements,\
		-readability-function-cognitive-complexity,\
		-*.insecureAPI.rand,\
		-cert-msc30-c,\
		-cert-msc50-cpp,\
		-cert-msc32-c,\
		-cert-msc51-cpp,\
		-hicpp-signed-bitwise
LINT_FILTERS=$(LINT_INCL),$(LINT_EXCL)

COMMON_C=tape.c
COMMON_O=bin/tape.o

all: bin/tape

# static analysis
check: $(COMMON_C)
	# clang-check -analyze $^ -- $(CFLAGS)
	clang-tidy $^ -checks='$(LINT_FILTERS)' -- $(CFLAGS)

# dynamic analysis
test: bin/tape
	bin/tape
	# check for memory leaks, OSX specific and ANNOYING (codesign and other BS), just use valgrind instead if you can
	leaks --atExit -- $< -q

bin/%: bin/%.o $(COMMON_O)
	clang $(CFLAGS) $^ -o $@

bin/%.o: %.c
	@ mkdir -p bin/
	clang $(CFLAGS) -c $< -o $@

clean:
	rm -r bin/
