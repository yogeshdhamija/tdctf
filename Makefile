SHELL := /bin/bash

EMSDK_DIR := src/platform/emsdk
EMSDK_ENV := $(EMSDK_DIR)/emsdk_env.sh

GAME_SRCS := src/game/game.c src/game/tower_config.c src/game/creep_config.c src/game/map_config.c

SRCS := $(GAME_SRCS) src/render/render.c src/platform/platform_web.c
SHELL_HTML := src/platform/shell.html
OUT := build/index.html

TOWER_CONFIG_SRC := data/towers.cfg
TOWER_CONFIG_HDR := build/tower_config_data.h
CREEP_CONFIG_SRC := data/creep_upgrades.cfg
CREEP_CONFIG_HDR := build/creep_config_data.h
MAP_CONFIG_SRC   := data/map.cfg
MAP_CONFIG_HDR   := build/map_config_data.h

INCLUDES := -Isrc -Ibuild

EMFLAGS := -O2 -s WASM=1 -s SINGLE_FILE=1 --shell-file $(SHELL_HTML) \
           -s EXPORTED_RUNTIME_METHODS=UTF8ToString

TEST_PATHING_BIN := build/test_pathing
TEST_PATHING_SRCS := tests/test_pathing.c $(GAME_SRCS)
TEST_GAME_BIN := build/test_game
TEST_GAME_SRCS := tests/test_game.c $(GAME_SRCS)
TEST_RENDER_BIN := build/test_render
TEST_RENDER_SRCS := tests/test_render.c src/render/render.c $(GAME_SRCS)
TEST_TOWER_CONFIG_BIN := build/test_tower_config
TEST_TOWER_CONFIG_SRCS := tests/test_tower_config.c src/game/tower_config.c
TEST_CREEP_CONFIG_BIN := build/test_creep_config
TEST_CREEP_CONFIG_SRCS := tests/test_creep_config.c src/game/creep_config.c
TEST_MAP_CONFIG_BIN := build/test_map_config
TEST_MAP_CONFIG_SRCS := tests/test_map_config.c src/game/map_config.c

.PHONY: all clean setup test

all: $(OUT)

test: $(TEST_PATHING_BIN) $(TEST_GAME_BIN) $(TEST_RENDER_BIN) $(TEST_TOWER_CONFIG_BIN) $(TEST_CREEP_CONFIG_BIN) $(TEST_MAP_CONFIG_BIN)
	$(TEST_PATHING_BIN)
	$(TEST_GAME_BIN)
	$(TEST_RENDER_BIN)
	$(TEST_TOWER_CONFIG_BIN)
	$(TEST_CREEP_CONFIG_BIN)
	$(TEST_MAP_CONFIG_BIN)

$(TOWER_CONFIG_HDR): $(TOWER_CONFIG_SRC) | build
	@echo 'Generating $@'
	@{ \
	  echo '/* Generated from $(TOWER_CONFIG_SRC) by Makefile. Do not edit. */'; \
	  echo '#ifndef TOWER_CONFIG_DATA_H'; \
	  echo '#define TOWER_CONFIG_DATA_H'; \
	  echo 'static const char TOWER_CONFIG_DEFAULT[] ='; \
	  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/.*/"&\\n"/' $<; \
	  echo ';'; \
	  echo '#endif'; \
	} > $@

$(CREEP_CONFIG_HDR): $(CREEP_CONFIG_SRC) | build
	@echo 'Generating $@'
	@{ \
	  echo '/* Generated from $(CREEP_CONFIG_SRC) by Makefile. Do not edit. */'; \
	  echo '#ifndef CREEP_CONFIG_DATA_H'; \
	  echo '#define CREEP_CONFIG_DATA_H'; \
	  echo 'static const char CREEP_UPGRADE_CONFIG_DEFAULT[] ='; \
	  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/.*/"&\\n"/' $<; \
	  echo ';'; \
	  echo '#endif'; \
	} > $@

$(MAP_CONFIG_HDR): $(MAP_CONFIG_SRC) | build
	@echo 'Generating $@'
	@{ \
	  echo '/* Generated from $(MAP_CONFIG_SRC) by Makefile. Do not edit. */'; \
	  echo '#ifndef MAP_CONFIG_DATA_H'; \
	  echo '#define MAP_CONFIG_DATA_H'; \
	  echo 'static const char MAP_CONFIG_DEFAULT[] ='; \
	  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/.*/"&\\n"/' $<; \
	  echo ';'; \
	  echo '#endif'; \
	} > $@

$(TEST_PATHING_BIN): $(TEST_PATHING_SRCS) $(TOWER_CONFIG_HDR) $(CREEP_CONFIG_HDR) $(MAP_CONFIG_HDR) src/game/game.h src/game/tower_config.h src/game/creep_config.h src/game/map_config.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) $(TEST_PATHING_SRCS) -o $(TEST_PATHING_BIN)

$(TEST_GAME_BIN): $(TEST_GAME_SRCS) $(TOWER_CONFIG_HDR) $(CREEP_CONFIG_HDR) $(MAP_CONFIG_HDR) src/game/game.h src/game/tower_config.h src/game/creep_config.h src/game/map_config.h tests/test_fixtures.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) -Itests $(TEST_GAME_SRCS) -o $(TEST_GAME_BIN)

$(TEST_RENDER_BIN): $(TEST_RENDER_SRCS) $(TOWER_CONFIG_HDR) $(CREEP_CONFIG_HDR) $(MAP_CONFIG_HDR) src/game/game.h src/render/render.h src/platform/platform.h tests/test_fixtures.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) -Itests $(TEST_RENDER_SRCS) -o $(TEST_RENDER_BIN)

$(TEST_TOWER_CONFIG_BIN): $(TEST_TOWER_CONFIG_SRCS) $(TOWER_CONFIG_HDR) src/game/tower_config.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) $(TEST_TOWER_CONFIG_SRCS) -o $(TEST_TOWER_CONFIG_BIN)

$(TEST_CREEP_CONFIG_BIN): $(TEST_CREEP_CONFIG_SRCS) $(CREEP_CONFIG_HDR) src/game/creep_config.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) $(TEST_CREEP_CONFIG_SRCS) -o $(TEST_CREEP_CONFIG_BIN)

$(TEST_MAP_CONFIG_BIN): $(TEST_MAP_CONFIG_SRCS) $(MAP_CONFIG_HDR) src/game/map_config.h | build
	cc -O0 -g -Wall -Wextra $(INCLUDES) $(TEST_MAP_CONFIG_SRCS) -o $(TEST_MAP_CONFIG_BIN)

$(OUT): $(SRCS) $(TOWER_CONFIG_HDR) $(CREEP_CONFIG_HDR) $(MAP_CONFIG_HDR) $(SHELL_HTML) | build
	source $(EMSDK_ENV) > /dev/null 2>&1 && emcc $(SRCS) -o $(OUT) $(EMFLAGS) $(INCLUDES)

build:
	mkdir -p build

setup:
	git clone https://github.com/emscripten-core/emsdk.git $(EMSDK_DIR)
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

clean:
	rm -rf build
