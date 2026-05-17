CC = cc
CFLAGS = -g -Wall 
LDFLAGS = -lSDL3 -lvulkan -lm
BROWSER = vivaldi

WIN_CC = x86_64-w64-mingw32-gcc
WIN_CFLAGS = -Wall -fstack-protector
WIN_LDFLAGS = -static -static-libgcc \
              -Wl,-Bstatic -lSDL3 \
              -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 \
              -lole32 -loleaut32 -lversion -luuid -ladvapi32 \
              -lsetupapi -lshell32 -ldinput8 \
              -Wl,-Bdynamic -lvulkan-1 \
              -mwindows

linux: bin/main bin/shaders/select.spv bin/shaders/model_vertex.spv bin/shaders/fx_fragment.spv bin/shaders/fx_vertex.spv bin/shaders/model_fragment.spv bin/shaders/ui_vertex.spv bin/shaders/ui_fragment.spv bin/assets

windows: bin/main.exe bin/shaders/model_vertex.spv bin/shaders/fx_fragment.spv bin/shaders/fx_vertex.spv bin/shaders/model_fragment.spv bin/shaders/ui_vertex.spv bin/shaders/ui_fragment.spv bin/assets

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

bin/shaders/ui_fragment.spv: src/shaders/ui.frag | bin
	glslc $< -o $@

bin/shaders/ui_vertex.spv: src/shaders/ui.vert | bin
	glslc $< -o $@

bin/shaders/select.spv: src/shaders/distinct.comp | bin
	glslc $< -o $@

bin/assets: assets/* scripts/copy-assets.sh | bin
	./scripts/copy-assets.sh assets bin/assets

profile: export DEBUGINFOD_URLS=https://debuginfod.archlinux.org
profile: export CFLAGS+=-fno-omit-frame-pointer
profile: linux
	cd ./bin && \
	perf record --call-graph dwarf,16384 -F 99 ./main && \
	perf script | inferno-collapse-perf | inferno-flamegraph > flamegraph.svg && \
	$(BROWSER) ./flamegraph.svg

bin:
	mkdir -p bin bin/assets bin/shaders

run: linux
	cd ./bin && mangohud ./main

clean:
	rm -rf bin

.PHONY: linux windows run clean opt-check
