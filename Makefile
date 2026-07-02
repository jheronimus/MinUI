CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Werror -pedantic -Isrc

all: main test_runner

main: src/main.c
	$(CC) $(CFLAGS) src/main.c -o main

test_runner: tests/test_dummy.c
	$(CC) $(CFLAGS) tests/test_dummy.c -o test_runner

clean:
	rm -f main test_runner
