CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config --cflags sdl3 vulkan)
LDLIBS  = $(shell pkg-config --libs sdl3 vulkan)

test: test.c
	$(CC) $(CFLAGS) test.c -o test $(LDLIBS)

clean:
	rm -f test

.PHONY: clean