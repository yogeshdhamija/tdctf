SHELL := /bin/bash

EMSDK_DIR := src/platform/emsdk
EMSDK_ENV := $(EMSDK_DIR)/emsdk_env.sh

SRCS := src/game/game.c src/render/render.c src/platform/platform_web.c
SHELL_HTML := src/platform/shell.html
OUT := build/index.html

EMFLAGS := -O2 -s WASM=1 -s SINGLE_FILE=1 --shell-file $(SHELL_HTML) \
           -s EXPORTED_RUNTIME_METHODS=UTF8ToString

TEST_BIN := build/test_pathing
TEST_SRCS := tests/test_pathing.c src/game/game.c

.PHONY: all clean setup test

all: $(OUT)

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) src/game/game.h | build
	cc -O0 -g -Wall -Wextra -Isrc $(TEST_SRCS) -o $(TEST_BIN)

$(OUT): $(SRCS) $(SHELL_HTML) | build
	source $(EMSDK_ENV) > /dev/null 2>&1 && emcc $(SRCS) -o $(OUT) $(EMFLAGS) -Isrc

build:
	mkdir -p build

setup:
	git clone https://github.com/emscripten-core/emsdk.git $(EMSDK_DIR)
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

clean:
	rm -rf build
