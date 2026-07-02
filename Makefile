CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Werror -pedantic -Isrc

all: test_runner

test_runner: tests/test_dummy.c
	$(CC) $(CFLAGS) tests/test_dummy.c -o test_runner

clean:
	rm -f test_runner
