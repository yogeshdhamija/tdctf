SHELL := /bin/bash

EMSDK_DIR := src/platform/emsdk
EMSDK_ENV := $(EMSDK_DIR)/emsdk_env.sh

SRCS := src/game/game.c src/render/render.c src/platform/platform_web.c
SHELL_HTML := src/platform/shell.html
OUT := build/index.html

EMFLAGS := -O2 -s WASM=1 -s SINGLE_FILE=1 --shell-file $(SHELL_HTML) \
           -s EXPORTED_RUNTIME_METHODS=UTF8ToString

TEST_PATHING_BIN := build/test_pathing
TEST_PATHING_SRCS := tests/test_pathing.c src/game/game.c
TEST_GAME_BIN := build/test_game
TEST_GAME_SRCS := tests/test_game.c src/game/game.c
TEST_RENDER_BIN := build/test_render
TEST_RENDER_SRCS := tests/test_render.c src/render/render.c src/game/game.c

.PHONY: all clean setup test

all: $(OUT)

test: $(TEST_PATHING_BIN) $(TEST_GAME_BIN) $(TEST_RENDER_BIN)
	$(TEST_PATHING_BIN)
	$(TEST_GAME_BIN)
	$(TEST_RENDER_BIN)

$(TEST_PATHING_BIN): $(TEST_PATHING_SRCS) src/game/game.h | build
	cc -O0 -g -Wall -Wextra -Isrc $(TEST_PATHING_SRCS) -o $(TEST_PATHING_BIN)

$(TEST_GAME_BIN): $(TEST_GAME_SRCS) src/game/game.h | build
	cc -O0 -g -Wall -Wextra -Isrc $(TEST_GAME_SRCS) -o $(TEST_GAME_BIN)

$(TEST_RENDER_BIN): $(TEST_RENDER_SRCS) src/game/game.h src/render/render.h src/platform/platform.h | build
	cc -O0 -g -Wall -Wextra -Isrc $(TEST_RENDER_SRCS) -o $(TEST_RENDER_BIN)

$(OUT): $(SRCS) $(SHELL_HTML) | build
	source $(EMSDK_ENV) > /dev/null 2>&1 && emcc $(SRCS) -o $(OUT) $(EMFLAGS) -Isrc

build:
	mkdir -p build

setup:
	git clone https://github.com/emscripten-core/emsdk.git $(EMSDK_DIR)
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

clean:
	rm -rf build
