# This file exists ONLY so CLion (and other CMake-aware IDEs) can index the
# codebase, including platform_web.c which #includes <emscripten.h>. The real
# build is driven by the Makefile via emcc — do not use CMake to build.
#
# After running `make setup`, emsdk is installed at src/platform/emsdk/ and
# CLion will resolve <emscripten.h> against the path below.

cmake_minimum_required(VERSION 3.20)
project(tdctf C)

set(CMAKE_C_STANDARD 11)

include_directories(
    src
    build
    src/platform/emsdk/upstream/emscripten/system/include
)

# Indexing-only target. Lists every source so CLion picks them up.
add_executable(tdctf_index_only
    src/game/game.c
    src/game/tower_config.c
    src/render/render.c
    src/platform/platform_web.c
)
