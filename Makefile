CFLAGS=-O3 -ffast-math -Wall -Wextra -std=c99

all: bin/tm

check: tm.c
	# clang-format --dry-run -Werror tm.c
	# clang-tdiy tm.c

bin/test: bin/test.o bin/tm.o
	clang $(CFLAGS) $^ -o $@
	# test for memory leaks, OSX specific (can also use e.g. valgrind)
	leaks --atExit -- bin/test

bin/%.o: %.c bin/
	clang $(CFLAGS) $< -o $@

bin/:
	mkdir bin/

clean:
	rm -r bin/
