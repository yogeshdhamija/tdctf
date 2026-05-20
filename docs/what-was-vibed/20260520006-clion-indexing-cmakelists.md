# CLion indexing fix: stub CMakeLists.txt

CLion couldn't resolve `<emscripten.h>` in `src/platform/platform_web.c`
because the file is only ever compiled with `emcc`, which injects the emsdk
include path. CLion's default toolchain has no knowledge of emsdk, so the
include failed and indexing/IntelliSense was broken.

Added a stub `CMakeLists.txt` at the repo root. CLion auto-detects it on
project open and uses it as the indexing model. The file lists `src`,
`build`, and `src/platform/emsdk/upstream/emscripten/system/include` as
include dirs, plus an indexing-only executable target that names every
source file.

The real build is unchanged — still `make` (which invokes `emcc`). CMake is
**not** wired up to actually build anything; it exists purely for IDE
indexing.

Caveat: `src/platform/emsdk/` is `.gitignore`'d and only appears after
`make setup`. Before setup, CLion will still flag `<emscripten.h>` as
missing. After setup, it resolves. We don't vendor emsdk (~1GB+) for this.
