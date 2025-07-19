CFLAGS=-O0 -ffast-math -Wall -Wextra -std=c99 -g

all: bin/test bin/vis

# static analysis
check: tm.c
	# clang-format --dry-run -Werror tm.c
	# clang-tdiy tm.c

# dynamic analysis
test: bin/test
	# check for memory leaks, OSX specific (can also use e.g. valgrind)
	leaks --atExit -- bin/test

bin/vis: bin/vis.o bin/tm.o
	clang $(CFLAGS) $^ -o $@

bin/test: bin/test.o bin/tm.o
	clang $(CFLAGS) $^ -o $@

bin/%.o: %.c bin/
	clang $(CFLAGS) -c $< -o $@

bin/:
	mkdir bin/

clean:
	rm -r bin/
