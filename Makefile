SHELL := /bin/bash

EMSDK_DIR := src/platform/emsdk
EMSDK_ENV := $(EMSDK_DIR)/emsdk_env.sh

SRCS := src/game/game.c src/render/render.c src/platform/platform_web.c
SHELL_HTML := src/platform/shell.html
OUT := build/index.html

EMFLAGS := -O2 -s WASM=1 --shell-file $(SHELL_HTML)

.PHONY: all clean serve setup

all: $(OUT)

$(OUT): $(SRCS) $(SHELL_HTML) | build
	source $(EMSDK_ENV) > /dev/null 2>&1 && emcc $(SRCS) -o $(OUT) $(EMFLAGS) -Isrc

build:
	mkdir -p build

setup:
	git clone https://github.com/emscripten-core/emsdk.git $(EMSDK_DIR)
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

serve: $(OUT)
	@echo "Serving at http://localhost:8080"
	python3 -m http.server 8080 --directory build

clean:
	rm -rf build
