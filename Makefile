CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config --cflags sdl3 vulkan)
LDLIBS  = $(shell pkg-config --libs sdl3 vulkan)

test: assets/test.c
	$(CC) $(CFLAGS) assets/test.c -o bin/test $(LDLIBS)

clean:
	rm -f test

.PHONY: clean