.PRECIOUS: bin/%.o

### Disabled some compiler warnings ###
# declaration-after-statement				I use C99 and don't need C89 compatibility
# tentative-definition-compat				I don't need C++ compatibility
# implicit-void-ptr-cast					This is a language feature, explicit casts are just noise
# unsafe-buffer-usage						All buffer usage in C is (arguably) unsafe
# unused-function							For development, will enable later
WFLAGS_EXCL=-Wno-declaration-after-statement -Wno-tentative-definition-compat -Wno-implicit-void-ptr-cast -Wno-unsafe-buffer-usage -Wno-unused-function
# Incldue as many compiler warnings as possible
WFLAGS=-std=c99 -pedantic -ferror-limit=0 -Weverything -Wno-padded $(WFLAGS_EXCL)
SANFLAGS=-fsanitize=address,leak,undefined,implicit-conversion,local-bounds,nullability
# Debugging symbols
CFLAGS_DEBUG=$(WFLAGS) -O3 -g3 -ffast-math $(SANFLAGS)
# Optimized build to match release, but still with sanitization for testing
CFLAGS_TEST=$(WFLAGS) -O3 -ffast-math $(SANFLAGS)
# Compiling for the fastest possible executable, skipping asserts and sanitizers
CFLAGS_RELEASE=$(WFLAGS) -O3 -ffast-math -DNDEBUG

# Include as many linter checks as possible
LINT_INCL=bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,hicpp-*,linuxkernel-*,llvm-*,misc-*,performance-*,portability-*,readability-*
### Disabled some linter checks ###
# bugprone-easily-swappable-parameters			This is too much work to "fix", and most functions have a logical paramter order.
# readability-identifier-length					I prefer maximum(a, b) to maximum(num1, num2).
# readability-math-missing-parentheses			I know PEMDAS.
# *-braces-around-statements					I use K&R coding style.
# readability-function-cognitive-complexity		I don't like to split up functions unneccesarily
# clang-diagnostic-unused-macros				This does not seem to work at all, false positives for header guards and macros used in other files
# llvm-header-guard								I just haven't found out how to make this not require the entire path (from /)
LINT_EXCL=\
		-bugprone-easily-swappable-parameters,\
		-readability-identifier-length,\
		-readability-math-missing-parentheses,\
		-*-braces-around-statements,\
		-readability-function-cognitive-complexity,\
		-clang-diagnostic-unused-macros,\
		-llvm-header-guard
LINT_FILTERS=$(LINT_INCL),$(LINT_EXCL)

ALL_C=$(wildcard *.c)
ALL_H=$(wildcard *.h)

NOT_VERIFIED=tape_bit.h tape_bit.c
VERIFIED_C=$(filter-out $(NOT_VERIFIED),$(ALL_C))
VERIFIED_H=$(filter-out $(NOT_VERIFIED),$(ALL_H))

COMMON_C=tm_run.c tm_def.c tape.c tape_flat.c tape_rle.c tape_bit.c util.c test_case.c
COMMON_H=$(subst .c,.h,$(COMMON_C))

# main target: simply compile the debug binary
all: bin/tst_test bin/tst_comp

# static analysis of all source files
check: $(VERIFIED_C) $(VERIFIED_H)
	clang-tidy $^ -checks='$(LINT_FILTERS)' -- $(CFLAGS_DEBUG)

# dynamic analysis of certain binaries
test: bin/tst_test
	bin/tst_test -f -r -b -c

# launches debugger
debug: bin/dbg_test
	lldb bin/dbg_test

# runs benchmarking on the fastest executable
bench: bin/rel_test
	bin/rel_test -f -q
	bin/rel_test -r -q
	bin/rel_test -b -q

bin/tst_%: %.c $(COMMON_C) $(COMMON_H)
	@ mkdir -p bin/
	clang $(CFLAGS_TEST) $< $(COMMON_C) -o $@

bin/dbg_%: %.c $(COMMON_C) $(COMMON_H)
	@ mkdir -p bin/
	clang $(CFLAGS_DEBUG) $< $(COMMON_C) -o $@

bin/rel_%: %.c $(COMMON_C) $(COMMON_H)
	@ mkdir -p bin/
	clang $(CFLAGS_RELEASE) $< $(COMMON_C) -o $@

clean:
	rm -r bin/
