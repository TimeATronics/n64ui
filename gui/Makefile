# N64UI - TrimUI Brick mupen64plus frontend
#
# Build targets:
#   make release   device build via the docker toolchain image (aarch64)
#   make local     device build, direct WSL cross-compile (no docker)
#   make host      native x86_64 build for the WSL host (SDL window, dev only)
#   make clean
#
# One source tree runs on both the Brick and the host; platform selection is
# runtime (util/Platform.cpp), like music_player. Device builds produce
# dist/n64ui (aarch64), host builds dist/n64ui-host (x86_64).
# Sample game for host dev: ../SuperMario64.z64
# All text is rendered with SDL_ttf (device build must link SDL2_ttf).

CROSS_COMPILE ?= aarch64-linux-gnu-
SYSROOT ?= /usr/aarch64-linux-gnu
API_INC ?= ../mupen64plus-builder/src/mupen64plus-core/src/api

OPT = -O3 -mcpu=cortex-a53
CXXSTD = -std=c++17
WARN = -Wall -Wextra

CXX = $(CROSS_COMPILE)g++

# all sources compile on both platforms (Renderer/VidExt pick a backend at
# runtime via Platform::isDevice())
SRCS = $(shell find src -name '*.cpp' | sort)
OBJS = $(patsubst src/%.cpp,dist/obj/%.o,$(SRCS))
OUT = dist/n64ui

INC = -Isrc -I$(API_INC) \
      -I$(SYSROOT)/usr/include -I$(SYSROOT)/usr/include/SDL2
CFLAGS = $(OPT) $(CXXSTD) $(WARN) -fPIC -D_REENTRANT --sysroot=$(SYSROOT)
LIBS = -L$(SYSROOT)/usr/lib -lSDL2 -lSDL2_ttf -lEGL -lGLESv2 -ldl -lpthread

.PHONY: all release local host font clean

all: $(OUT)

$(OUT): $(OBJS)
	@mkdir -p dist
	$(CXX) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

dist/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<

# ---- device build via docker toolchain --------------------------------------
release:
	bash scripts/build-docker.sh

# ---- device build, direct WSL cross-compile (fast iteration) -----------------
local: clean
	$(MAKE) -j2 all CROSS_COMPILE=/home/aradhya/toolchains/gcc16-aarch64/bin/aarch64-linux-gnu- \
	        SYSROOT=/home/aradhya/trimui-sysroot

# ---- native host (WSL) dev build --------------------------------------------
HOST_CXX ?= g++
HOST_INC = -Isrc -I$(API_INC) $(shell pkg-config --cflags sdl2 egl glesv2 2>/dev/null)
HOST_LIBS = $(shell pkg-config --libs sdl2 egl glesv2 SDL2_ttf 2>/dev/null) \
            -l:libmupen64plus.so.2 -ldl -lpthread
HOST_OBJS = $(patsubst src/%.cpp,dist/hobj/%.o,$(SRCS))

dist/hobj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXSTD) -O2 -g $(WARN) -DHOST_BUILD $(HOST_INC) -c -o $@ $<

host: $(HOST_OBJS)
	@mkdir -p dist
	$(HOST_CXX) -o dist/n64ui-host $(HOST_OBJS) $(HOST_LIBS)

# run the host build on the WSL desktop (builds first if missing)
#   make run [ROM=path]
run: host
	@mkdir -p Roms
	@test -f dist/n64ui-host && ./dist/n64ui-host $(ROM)

clean:
	rm -rf dist
