all: bin/tm

bin/%: %.c bin/
	gcc -O3 -Wall -Wextra -std=c99 $< -o $@

bin/:
	mkdir bin/

clean:
	rm bin/*
