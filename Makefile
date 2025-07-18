all: bin/tm_easy bin/tm

test: all
	bin/tm_easy 1RB1LB_1LA1RZ
	bin/tm 1RB1LB_1LA1RZ

bin/%: %.c
	gcc -Wall -Wextra -std=c99 $< -o $@

bin/%: %.s
	clang -arch arm64 $< -o $@

bin/:
	mkdir bin/
