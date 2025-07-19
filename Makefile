CFLAGS=-O0 -ffast-math -Wall -Wextra -std=c99 -g

all: bin/test

check: tm.c
	# clang-format --dry-run -Werror tm.c
	# clang-tdiy tm.c
	# test for memory leaks, OSX specific (can also use e.g. valgrind)
	# leaks --atExit -- bin/test

bin/test: bin/test.o bin/tm.o
	clang $(CFLAGS) $^ -o $@

bin/%.o: %.c bin/
	clang $(CFLAGS) -c $< -o $@

bin/:
	mkdir bin/

clean:
	rm -r bin/
