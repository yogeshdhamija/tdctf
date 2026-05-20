# CLion indexing fix: compile_flags.txt

CLion couldn't resolve `<emscripten.h>` in `src/platform/platform_web.c`
because the file is only ever compiled with `emcc`, which injects the emsdk
include path. CLion's default code-insight has no knowledge of emsdk, so
the include failed and indexing/IntelliSense was broken.

First attempted a stub `CMakeLists.txt`, but the project is opened in
CLion as a **Makefile project** (see `.idea/misc.xml`
`<component name="MakefileSettings">`), which ignores `CMakeLists.txt`
entirely. CLion's Makefile parser also doesn't recognize `emcc` as a
compiler, so it can't extract include paths from the Makefile either.

Fix: committed a `compile_flags.txt` at the repo root. This is a clangd
config file — clangd (which CLion uses for code insight) reads it
automatically regardless of project type. Each line is one flag applied to
every C source. We include `src`, `build`, and the emsdk include path.

The real build is unchanged — still `make` (which invokes `emcc`).
`compile_flags.txt` exists purely for IDE indexing.

Caveat: `src/platform/emsdk/` is `.gitignore`'d and only appears after
`make setup`. Before setup, CLion will still flag `<emscripten.h>` as
missing. After setup, it resolves. We don't vendor emsdk (~1GB+) for this.
