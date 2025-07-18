all: bin/tm

bin/%: %.c bin/
	gcc -Wall -Wextra -std=c99 $< -o $@

bin/:
	mkdir bin/
