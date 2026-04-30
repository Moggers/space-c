CC = cc
CFLAGS = -g -Wall  -O3 
LDFLAGS = -lSDL3 -lvulkan -lm 
all: bin/main bin/shaders/model_vertex.spv bin/shaders/fx_fragment.spv bin/shaders/fx_vertex.spv bin/shaders/model_fragment.spv bin/assets

bin/main: src/* | bin
	$(CC) $(CFLAGS) -o $@ ./src/main.c $(LDFLAGS)

opt-check: 
	$(CC) $(CFLAGS) -fopt-info-vec-missed=stderr -o $@ ./src/main.c $(LDFLAGS)

bin/shaders/model_fragment.spv: src/shaders/model.frag | bin
	glslc $< -o $@

bin/shaders/model_vertex.spv: src/shaders/model.vert | bin
	glslc $< -o $@

bin/shaders/fx_fragment.spv: src/shaders/fx.frag | bin
	glslc $< -o $@

bin/shaders/fx_vertex.spv: src/shaders/fx.vert | bin
	glslc $< -o $@

bin/assets: assets | bin
	cp -r assets bin/assets

bin:
	mkdir -p bin bin/assets bin/shaders

run: all
	./bin/main

clean:
	rm -rf bin

.PHONY: all run clean opt-check
