CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config --cflags sdl3 vulkan)
LDLIBS  = $(shell pkg-config --libs sdl3 vulkan)
 
bin/test: assets/test.c assets/device.c assets/device.h assets/swapchain.c assets/swapchain.h
	mkdir -p bin
	$(CC) $(CFLAGS) assets/test.c assets/device.c assets/swapchain.c -o bin/test $(LDLIBS)
 
clean:
	rm -f bin/test
 
.PHONY: clean
 
