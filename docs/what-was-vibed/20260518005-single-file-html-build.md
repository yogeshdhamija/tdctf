# Single-File HTML Build

## What changed
Build now produces one self-contained `build/index.html` that runs from `file://` — no server needed. Intended for sharing the game by sending a single file.

## Files modified
- `Makefile` — Added `-s SINGLE_FILE=1` to `EMFLAGS`. Removed the `serve` target and its `.PHONY` entry (no longer needed; the build is double-clickable).
- `README.md` — Removed the `make serve` step. Documents that the output is a single double-clickable HTML.

## Design decisions
- `SINGLE_FILE=1` base64-embeds the wasm into the HTML and inlines the loader JS. Slightly larger payload than separate files (~44KB at -O2), but eliminates the `fetch()` of `index.wasm` that breaks under `file://` in some browsers.
- Kept `WASM=1`; `SINGLE_FILE` just changes packaging, not the codegen target.
