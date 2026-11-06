CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

all: mmap_policy_test

mmap_policy_test: mmap_policy_test.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f mmap_policy_test

.PHONY: all clean
