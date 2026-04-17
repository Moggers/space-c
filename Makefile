CC = cc
CFLAGS = -g -Wall
LDFLAGS = -lSDL3 -lvulkan

all: bin/main bin/vertex.spv bin/fragment.spv

bin/main: src/main.c | bin
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bin/fragment.spv: src/shaders/test/fragment.frag | bin
	glslc $< -o $@

bin/vertex.spv: src/shaders/test/vertex.vert | bin
	glslc $< -o $@

bin:
	mkdir -p bin

run: all
	./bin/main

clean:
	rm -rf bin

.PHONY: all run clean
