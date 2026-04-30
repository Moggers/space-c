CC = cc
CFLAGS = -g -Wall  -O3
LDFLAGS = -lSDL3 -lvulkan -lm

WIN_CC = x86_64-w64-mingw32-gcc
WIN_CFLAGS = -Wall -O3
WIN_LDFLAGS = -static -static-libgcc \
              -Wl,-Bstatic -lSDL3 \
              -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 \
              -lole32 -loleaut32 -lversion -luuid -ladvapi32 \
              -lsetupapi -lshell32 -ldinput8 \
              -Wl,-Bdynamic -lvulkan-1 \
              -mwindows

all: bin/main bin/shaders/model_vertex.spv bin/shaders/fx_fragment.spv bin/shaders/fx_vertex.spv bin/shaders/model_fragment.spv bin/assets

windows: bin/main.exe bin/shaders/model_vertex.spv bin/shaders/fx_fragment.spv bin/shaders/fx_vertex.spv bin/shaders/model_fragment.spv bin/assets

bin/main: src/* | bin
	$(CC) $(CFLAGS) -o $@ ./src/main.c $(LDFLAGS)

bin/main.exe: src/* | bin
	$(WIN_CC) $(WIN_CFLAGS) -o $@ ./src/main.c $(WIN_LDFLAGS)

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

.PHONY: all windows run clean opt-check
