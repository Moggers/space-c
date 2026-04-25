CC = cc
CFLAGS = -g -Wall 
LDFLAGS = -lSDL3 -lvulkan -lm 
all: bin/main bin/shaders/model_vertex.spv bin/shaders/model_fragment.spv bin/assets

bin/main: src/* | bin
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bin/shaders/model_fragment.spv: src/shaders/model.frag | bin
	glslc $< -o $@

bin/shaders/model_vertex.spv: src/shaders/model.vert | bin
	glslc $< -o $@

bin/assets: assets | bin
	cp -r assets bin/assets

bin:
	mkdir -p bin bin/assets bin/shaders

run: all
	./bin/main

clean:
	rm -rf bin

.PHONY: all run clean
